#include <cassert>
#include <string>

#include "wifi_qr.h"

using tilehaus::wifi_qr_payload;

int main() {
  assert(wifi_qr_payload("Guests", "hunter22") == "WIFI:T:WPA;S:Guests;P:hunter22;;");
  // Open network: no password field.
  assert(wifi_qr_payload("Cafe", "") == "WIFI:T:nopass;S:Cafe;;");
  // Every special character escaped, in both fields.
  assert(wifi_qr_payload("My;Net", R"(a\b;c,d:e"f)") == R"(WIFI:T:WPA;S:My\;Net;P:a\\b\;c\,d\:e\"f;;)");
  // Spaces and UTF-8 pass through.
  assert(wifi_qr_payload("Café Guest", "mot de passe") == "WIFI:T:WPA;S:Café Guest;P:mot de passe;;");
  // No network name, no code.
  assert(wifi_qr_payload("", "x").empty());
  return 0;
}
