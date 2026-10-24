#pragma once
#include <string>

namespace tilehaus {

// The text a Wi-Fi QR code carries, in the format phone cameras recognise:
//   WIFI:T:WPA;S:<ssid>;P:<password>;;
// Backslash, semicolon, comma, colon and double quote are escaped with a
// backslash. An empty password is an open network (T:nopass). Pure, so the
// escaping is host-tested; a wrong escape gives a code that scans to the
// wrong password, which is hard to spot by looking.

inline std::string wifi_qr_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char ch : s) {
    if (ch == '\\' || ch == ';' || ch == ',' || ch == ':' || ch == '"') out.push_back('\\');
    out.push_back(ch);
  }
  return out;
}

inline std::string wifi_qr_payload(const std::string &ssid, const std::string &password) {
  if (ssid.empty()) return "";
  std::string out = "WIFI:T:";
  out += password.empty() ? "nopass" : "WPA";
  out += ";S:" + wifi_qr_escape(ssid) + ";";
  if (!password.empty()) out += "P:" + wifi_qr_escape(password) + ";";
  out += ";";
  return out;
}

}  // namespace tilehaus
