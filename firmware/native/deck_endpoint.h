#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <esp_http_server.h>
#include <lwip/sockets.h>

#include "lvgl.h"
#include "esphome/core/application.h"
#include "card_config.h"
#include "card_host.h"
#include "config_session.h"
#include "deck_document.h"
#include "deck_live.h"
#include "deck_store.h"
#include "deck_ui_endpoint.h"
#include "splash.h"

namespace tilehaus {

inline constexpr char kDeckMediaType[] = "application/vnd.tilehaus.deck";

// Set by a successful PUT on the httpd task; polled + cleared on the main loop
// by apply_pending_reboot(). Reboot-to-apply: the saved deck is rebuilt cleanly
// on the next boot, avoiding a live teardown of HA state subscriptions that
// ESPHome's API server cannot unsubscribe.
// Set by bindings.yaml: runs shortly before the reset, to darken the panel.
// Native code cannot reach an ESPHome light directly, so the YAML supplies it.
inline std::function<void()> &pre_reboot_hook() {
  static std::function<void()> hook;
  return hook;
}

inline std::atomic<bool> &deck_reboot_pending() {
  static std::atomic<bool> pending{false};
  return pending;
}

// A saved deck that only changes how tiles look is applied in place instead:
// the PUT handler (httpd task) decides against the last deck it accepted, and
// hands the deck to the main loop, which rebuilds those tiles (apply_live_deck).
struct LiveQueue {
  std::mutex m;
  bool have_base = false;  // `base` is the deck on screen, or about to be
  DeckParts base;
  bool pending = false;
  DeckParts next;
};
inline LiveQueue &live_queue() {
  static LiveQueue q;
  return q;
}

// Called once the boot build is done, with the deck it built.
inline void live_queue_seed() {
  LiveQueue &q = live_queue();
  std::lock_guard<std::mutex> lock(q.m);
  q.have_base = build_context().built;
  q.base = build_context().deck;
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

// The client's address, for the HA status text. With IPv6 enabled, IPv4
// clients reach a dual-stack server as v4-mapped IPv6 (::ffff:a.b.c.d); those
// print as plain v4.
inline void peer_ip(httpd_req_t *req, char *out, size_t n) {
  out[0] = '\0';
  sockaddr_storage addr{};
  socklen_t len = sizeof(addr);
  if (getpeername(httpd_req_to_sockfd(req), reinterpret_cast<sockaddr *>(&addr), &len) != 0) return;
  if (addr.ss_family == AF_INET) {
    inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in *>(&addr)->sin_addr, out, n);
  }
#if LWIP_IPV6  // off in this panel's build; kept for boards that enable it
  else if (addr.ss_family == AF_INET6) {
    const uint8_t *b = reinterpret_cast<sockaddr_in6 *>(&addr)->sin6_addr.s6_addr;
    static const uint8_t kMapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
    if (std::memcmp(b, kMapped, sizeof(kMapped)) == 0) {
      inet_ntop(AF_INET, b + 12, out, n);
    } else {
      inet_ntop(AF_INET6, b, out, n);
    }
  }
#endif
}

// The raw Authorization value, or false when absent or too long to be ours.
inline bool read_authorization(httpd_req_t *req, char (&buf)[64]) {
  const size_t len = httpd_req_get_hdr_value_len(req, "Authorization");
  if (len == 0 || len >= sizeof(buf)) return false;
  return httpd_req_get_hdr_value_str(req, "Authorization", buf, sizeof(buf)) == ESP_OK;
}

// Gate for every write. Sends the refusal itself and returns false; on true
// the caller carries on. Runs before the body is read: httpd discards an
// unread body once the handler returns.
inline bool require_session(httpd_req_t *req) {
  char auth[64];
  const bool has = read_authorization(req, auth);
  switch (config_session_authorize(has ? auth : nullptr)) {
    case AuthResult::Ok:
      return true;
    case AuthResult::Locked:
      httpd_resp_set_status(req, "423 Locked");
      httpd_resp_sendstr(req, "The configurator is locked. Turn on the Configurator switch in Home Assistant.");
      return false;
    case AuthResult::Unauthorized:
      break;
  }
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Bearer");
  httpd_resp_sendstr(req, config_session_state() == LockState::Claimed
                              ? "Another session is configuring this panel."
                              : "Claim a session first: POST /api/v1/session.");
  return false;
}

}  // namespace deck_endpoint_detail

// GET /api/v1/session — whether the panel is locked, open or claimed, and
// whether the caller's key (if any) is the claimed one. With a valid key this
// counts as use, so a configurator that checks in keeps its session alive.
inline esp_err_t session_get_handler(httpd_req_t *req) {
  using namespace deck_endpoint_detail;
  char auth[64];
  const bool has = read_authorization(req, auth);
  const bool mine = has && config_session_authorize(auth) == AuthResult::Ok;
  const char *state = "locked";
  switch (config_session_state()) {
    case LockState::Locked: state = "locked"; break;
    case LockState::Open: state = "open"; break;
    case LockState::Claimed: state = "claimed"; break;
  }
  char body[96];
  std::snprintf(body, sizeof(body), "{\"state\":\"%s\",\"mine\":%s,\"timeoutMinutes\":%lu}",
                state, mine ? "true" : "false",
                static_cast<unsigned long>(config_session_timeout_ms() / 60000u));
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_sendstr(req, body);
  return ESP_OK;
}

// POST /api/v1/session — claim the open panel. The first caller gets the key;
// everyone after gets 409 until the switch is turned off and on again.
inline esp_err_t session_post_handler(httpd_req_t *req) {
  using namespace deck_endpoint_detail;
  char ip[kClaimantLen];
  peer_ip(req, ip, sizeof(ip));
  char key[kConfigKeyHexLen + 1];
  switch (config_session_claim(ip, key)) {
    case ClaimResult::Granted: {
      char body[64];
      std::snprintf(body, sizeof(body), "{\"key\":\"%s\"}", key);
      httpd_resp_set_status(req, "201 Created");
      httpd_resp_set_type(req, "application/json");
      httpd_resp_set_hdr(req, "Cache-Control", "no-store");
      httpd_resp_sendstr(req, body);
      return ESP_OK;
    }
    case ClaimResult::Locked:
      httpd_resp_set_status(req, "423 Locked");
      httpd_resp_sendstr(req, "The configurator is locked. Turn on the Configurator switch in Home Assistant.");
      return ESP_OK;
    case ClaimResult::AlreadyClaimed:
      break;
  }
  httpd_resp_set_status(req, "409 Conflict");
  httpd_resp_sendstr(req, "Another session is configuring this panel.");
  return ESP_OK;
}

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
  // The newest card type this firmware renders, so a configurator or CLI can
  // refuse a deck the panel would reject before sending it.
  char tbuf[4];
  std::snprintf(tbuf, sizeof(tbuf), "%u", static_cast<unsigned>(kDeckMaxType));
  httpd_resp_set_hdr(req, "X-Deck-Max-Type", tbuf);
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
// apply. Needs the session key (see require_session), the binary media type
// and an If-Match generation.
inline esp_err_t deck_put_handler(httpd_req_t *req) {
  using namespace deck_endpoint_detail;
  char etag[16];  // must outlive the httpd_resp_send() in the branches below
  if (!require_session(req)) return ESP_OK;

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
  DeckParts next;
  std::string why;
  if (!decode_deck(body.data(), body.size(), next.cards, &next.accent, &next.pages,
                   &next.grid_cols, &next.grid_rows, &why, &next.page_back)) {
    const std::string msg = "Deck not saved: " + why;  // outlives the send below
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, msg.c_str());
    return ESP_OK;
  }

  const uint32_t generation = deck_store().save(body.data(), body.size());
  if (generation == 0) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_sendstr(req, "Deck could not be saved");
    return ESP_OK;
  }
  bool live = false;
  {
    LiveQueue &q = live_queue();
    std::lock_guard<std::mutex> lock(q.m);
    if (q.have_base && !deck_reboot_pending().load() &&
        deck_live_change(q.base, next).live) {
      live = true;
      q.base = next;
      q.next = std::move(next);
      q.pending = true;
    } else {
      q.have_base = false;  // a reboot is coming: nothing more applies live
    }
  }
  if (!live) deck_reboot_pending().store(true);
  set_etag(req, generation, etag);
  // Tells the configurator not to wait for a reboot.
  if (live) httpd_resp_set_hdr(req, "X-Deck-Applied", "live");
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
}

// DELETE /api/v1/config — clear the stored deck (If-Match guarded), then reboot
// so the panel comes back to the "Awaiting Configuration" screen.
inline esp_err_t deck_delete_handler(httpd_req_t *req) {
  using namespace deck_endpoint_detail;
  char etag[16];
  if (!require_session(req)) return ESP_OK;

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
  {
    std::lock_guard<std::mutex> lock(live_queue().m);
    live_queue().have_base = false;
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
  // The default cap is 8 URI handlers and this server registers 7.
  config.max_uri_handlers = 12;
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

  httpd_uri_t session_get_uri = {};
  session_get_uri.uri = "/api/v1/session";
  session_get_uri.method = HTTP_GET;
  session_get_uri.handler = session_get_handler;
  httpd_register_uri_handler(deck_httpd(), &session_get_uri);

  httpd_uri_t session_post_uri = {};
  session_post_uri.uri = "/api/v1/session";
  session_post_uri.method = HTTP_POST;
  session_post_uri.handler = session_post_handler;
  httpd_register_uri_handler(deck_httpd(), &session_post_uri);

  register_deck_ui_endpoints(deck_httpd());
}

// Main-loop hook: after a PUT persists a new deck, wait a few ticks so the 204
// response fully flushes to the client, then reboot to apply the deck cleanly at
// boot. Rebooting on the same tick the flag is set races the httpd response
// flush, so the client sees a dropped connection instead of its 204.
inline void apply_pending_reboot() {
  // Ticks are 200ms (see bindings.yaml). The full grace stays 8 ticks so the
  // HTTP response still flushes; the screen just goes dark partway through.
  static constexpr int kGraceTicks = 8;      // 1.6s total
  static constexpr int kDarkAtTick = 5;      // 600ms in: splash seen, then dark
  static int reboot_countdown = -1;          // -1 = idle; else ticks until reboot
  // A deck the PUT handler accepted as a live change: rebuild those tiles.
  if (reboot_countdown < 0 && !deck_reboot_pending().load()) {
    DeckParts next;
    bool take = false;
    {
      LiveQueue &q = live_queue();
      std::lock_guard<std::mutex> lock(q.m);
      if (q.pending) {
        next = std::move(q.next);
        q.pending = false;
        take = true;
      }
    }
    // Should not fail (same comparison), but if the screen and the queue
    // disagree, the stored deck still wins: apply it by reboot.
    if (take && !apply_live_deck(next)) deck_reboot_pending().store(true);
  }
  if (deck_reboot_pending().exchange(false)) {
    reboot_countdown = kGraceTicks;
    splash_set("\U000F06B0", "Rebooting", "Applying the new configuration");
    splash_show();
  }
  if (reboot_countdown > 0) {
    --reboot_countdown;
    // Kill the backlight before the reset so the boot's garbage frames are
    // never shown. hardware.yaml forces it back on at boot (restore_mode
    // ALWAYS_ON), so this cannot leave the panel dark.
    if (reboot_countdown == kDarkAtTick && pre_reboot_hook()) pre_reboot_hook()();
    if (reboot_countdown == 0) esphome::App.safe_reboot();
  }
}

}  // namespace tilehaus
