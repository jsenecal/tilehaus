#pragma once
#include <cstddef>
#include <cstdint>

#ifdef USE_ESP_IDF
#include <esp_http_server.h>

#include "deck_ui_asset.h"

namespace tilehaus {

// GET / — the HTML shell.
inline esp_err_t deck_ui_root_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, reinterpret_cast<const char *>(kDeckUiHtml),
                  static_cast<ssize_t>(kDeckUiHtmlLen));
  return ESP_OK;
}

// GET /app.js — the esbuild bundle, served gzip-compressed.
inline esp_err_t deck_ui_appjs_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/javascript");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  httpd_resp_send(req, reinterpret_cast<const char *>(kDeckUiAppJsGz),
                  static_cast<ssize_t>(kDeckUiAppJsGzLen));
  return ESP_OK;
}

inline void register_deck_ui_endpoints(httpd_handle_t server) {
  httpd_uri_t root_uri = {};
  root_uri.uri = "/";
  root_uri.method = HTTP_GET;
  root_uri.handler = deck_ui_root_handler;
  httpd_register_uri_handler(server, &root_uri);

  httpd_uri_t appjs_uri = {};
  appjs_uri.uri = "/app.js";
  appjs_uri.method = HTTP_GET;
  appjs_uri.handler = deck_ui_appjs_handler;
  httpd_register_uri_handler(server, &appjs_uri);
}

}  // namespace tilehaus
#endif  // USE_ESP_IDF
