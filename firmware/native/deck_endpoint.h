#pragma once
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <esp_http_server.h>

#include "lvgl.h"
#include "esphome/core/application.h"
#include "card_config.h"
#include "deck_document.h"
#include "deck_store.h"
#include "deck_ui_endpoint.h"

namespace tilehaus {

inline constexpr char kDeckMediaType[] = "application/vnd.tilehaus.deck";

// Set by a successful PUT on the httpd task; polled + cleared on the main loop
// by apply_pending_reboot(). Reboot-to-apply: the saved deck is rebuilt cleanly
// on the next boot, avoiding a live teardown of HA state subscriptions that
// ESPHome's API server cannot unsubscribe.
inline std::atomic<bool> &deck_reboot_pending() {
  static std::atomic<bool> pending{false};
  return pending;
}

inline httpd_handle_t &deck_httpd() {
  static httpd_handle_t handle = nullptr;
  return handle;
}

namespace deck_endpoint_detail {

// `buf` must outlive the httpd_resp_send() that follows: ESP-IDF's
// httpd_resp_set_hdr stores the pointer, not a copy, so the buffer has to live
// in the handler's scope (a local here would dangle before the response flushes).
inline void set_etag(httpd_req_t *req, uint32_t generation, char (&buf)[16]) {
  std::snprintf(buf, sizeof(buf), "\"%lu\"", static_cast<unsigned long>(generation));
  httpd_resp_set_hdr(req, "ETag", buf);
}

inline bool parse_etag(const char *value, uint32_t *out) {
  unsigned long parsed = 0;
  if (std::sscanf(value, "\"%lu\"", &parsed) != 1) return false;
  *out = static_cast<uint32_t>(parsed);
  return true;
}

}  // namespace deck_endpoint_detail

// GET /api/v1/config — current stored document + ETag, or 204 when empty.
inline esp_err_t deck_get_handler(httpd_req_t *req) {
  using namespace deck_endpoint_detail;
  DeckStore &store = deck_store();
  char etag[16];
  set_etag(req, store.generation(), etag);
  lv_display_t *disp = lv_display_get_default();
  char wbuf[8], hbuf[8];
  if (disp != nullptr) {
    std::snprintf(wbuf, sizeof(wbuf), "%d", static_cast<int>(lv_display_get_horizontal_resolution(disp)));
    std::snprintf(hbuf, sizeof(hbuf), "%d", static_cast<int>(lv_display_get_vertical_resolution(disp)));
    httpd_resp_set_hdr(req, "X-Display-Width", wbuf);
    httpd_resp_set_hdr(req, "X-Display-Height", hbuf);
  }
  if (store.empty()) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
  }
  httpd_resp_set_type(req, kDeckMediaType);
  httpd_resp_send(req, reinterpret_cast<const char *>(store.data()),
                  store.length());
  return ESP_OK;
}

// PUT /api/v1/config — validate + persist a deck document, then flag a live
// apply. Binary media type + If-Match generation, mirroring the shipping
// panel-config write endpoint.
inline esp_err_t deck_put_handler(httpd_req_t *req) {
  using namespace deck_endpoint_detail;
  char etag[16];  // must outlive the httpd_resp_send() in the branches below

  // Content-Type must be the deck media type.
  {
    size_t len = httpd_req_get_hdr_value_len(req, "Content-Type");
    char content_type[48];
    if (len == 0 || len >= sizeof(content_type) ||
        httpd_req_get_hdr_value_str(req, "Content-Type", content_type,
                                    sizeof(content_type)) != ESP_OK ||
        std::strcmp(content_type, kDeckMediaType) != 0) {
      httpd_resp_set_status(req, "415 Unsupported Media Type");
      httpd_resp_sendstr(req, "Deck config uses its binary media type");
      return ESP_OK;
    }
  }

  const int total = req->content_len;
  if (total <= 0 || static_cast<size_t>(total) > DeckStore::kMaxDocumentBytes) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "Invalid deck body");
    return ESP_OK;
  }

  // If-Match generation required, and must match the current generation.
  uint32_t expected = 0;
  {
    size_t len = httpd_req_get_hdr_value_len(req, "If-Match");
    char if_match[16];
    if (len == 0 || len >= sizeof(if_match) ||
        httpd_req_get_hdr_value_str(req, "If-Match", if_match,
                                    sizeof(if_match)) != ESP_OK ||
        !parse_etag(if_match, &expected)) {
      httpd_resp_set_status(req, "428 Precondition Required");
      httpd_resp_sendstr(req, "A quoted If-Match generation is required");
      return ESP_OK;
    }
  }
  if (expected != deck_store().generation()) {
    set_etag(req, deck_store().generation(), etag);
    httpd_resp_set_status(req, "409 Conflict");
    httpd_resp_sendstr(req, "Deck configuration changed on the device");
    return ESP_OK;
  }

  // Read the full body.
  std::vector<uint8_t> body(static_cast<size_t>(total));
  int received = 0;
  while (received < total) {
    int chunk = httpd_req_recv(req, reinterpret_cast<char *>(body.data()) + received,
                               total - received);
    if (chunk <= 0) {
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_sendstr(req, "Failed to read deck body");
      return ESP_OK;
    }
    received += chunk;
  }

  // Validate before persisting, so the store never holds an undecodable doc.
  std::vector<CardConfig> dry;
  if (!decode_deck(body.data(), body.size(), dry)) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "Malformed deck document");
    return ESP_OK;
  }

  const uint32_t generation = deck_store().save(body.data(), body.size());
  if (generation == 0) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_sendstr(req, "Deck could not be saved");
    return ESP_OK;
  }
  deck_reboot_pending().store(true);
  set_etag(req, generation, etag);
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

// DELETE /api/v1/config — clear the stored deck (If-Match guarded), then reboot
// so the panel comes back to the "Awaiting Configuration" screen.
inline esp_err_t deck_delete_handler(httpd_req_t *req) {
  using namespace deck_endpoint_detail;
  char etag[16];

  uint32_t expected = 0;
  {
    size_t len = httpd_req_get_hdr_value_len(req, "If-Match");
    char if_match[16];
    if (len == 0 || len >= sizeof(if_match) ||
        httpd_req_get_hdr_value_str(req, "If-Match", if_match,
                                    sizeof(if_match)) != ESP_OK ||
        !parse_etag(if_match, &expected)) {
      httpd_resp_set_status(req, "428 Precondition Required");
      httpd_resp_sendstr(req, "A quoted If-Match generation is required");
      return ESP_OK;
    }
  }
  if (expected != deck_store().generation()) {
    set_etag(req, deck_store().generation(), etag);
    httpd_resp_set_status(req, "409 Conflict");
    httpd_resp_sendstr(req, "Deck configuration changed on the device");
    return ESP_OK;
  }
  const uint32_t generation = deck_store().clear();
  if (generation == 0) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_sendstr(req, "Deck could not be cleared");
    return ESP_OK;
  }
  deck_reboot_pending().store(true);
  set_etag(req, generation, etag);
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

// Starts the dedicated deck-config HTTP server. Idempotent — safe to call every
// loop tick until networking is ready and httpd_start() succeeds.
inline void start_deck_server(uint16_t port = 80) {
  if (deck_httpd() != nullptr) return;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port;
  // The default 4 KB task stack is too tight for the deck handlers (body buffer
  // + document decode). Give it headroom so a request cannot overflow it.
  config.stack_size = 8192;
  if (httpd_start(&deck_httpd(), &config) != ESP_OK) {
    deck_httpd() = nullptr;
    return;
  }
  httpd_uri_t get_uri = {};
  get_uri.uri = "/api/v1/config";
  get_uri.method = HTTP_GET;
  get_uri.handler = deck_get_handler;
  httpd_register_uri_handler(deck_httpd(), &get_uri);

  httpd_uri_t put_uri = {};
  put_uri.uri = "/api/v1/config";
  put_uri.method = HTTP_PUT;
  put_uri.handler = deck_put_handler;
  httpd_register_uri_handler(deck_httpd(), &put_uri);

  httpd_uri_t delete_uri = {};
  delete_uri.uri = "/api/v1/config";
  delete_uri.method = HTTP_DELETE;
  delete_uri.handler = deck_delete_handler;
  httpd_register_uri_handler(deck_httpd(), &delete_uri);

  register_deck_ui_endpoints(deck_httpd());
}

// Main-loop hook: after a PUT persists a new deck, wait a few ticks so the 204
// response fully flushes to the client, then reboot to apply the deck cleanly at
// boot. Rebooting on the same tick the flag is set races the httpd response
// flush, so the client sees a dropped connection instead of its 204.
inline void apply_pending_reboot() {
  static int reboot_countdown = -1;  // -1 = idle; otherwise ticks until reboot
  if (deck_reboot_pending().exchange(false)) reboot_countdown = 8;  // ~1.6s grace
  if (reboot_countdown > 0 && --reboot_countdown == 0) {
    esphome::App.safe_reboot();
  }
}

}  // namespace tilehaus
