#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "timer_state.h"  // parse_iso8601, format_countdown

namespace tilehaus {

// A Home Assistant media_player, reduced to what the Media card draws. Works
// for any player integration, Music Assistant's included: they all report the
// same attributes.
//
// HA reports `media_position` only when playback changes (play, pause, seek),
// with `media_position_updated_at` saying when. So a playing track's position
// is counted on the panel from that moment. Pure, so it is host-tested.

enum class MediaPhase : uint8_t { Playing, Paused, Buffering, Idle, Off, Unavailable };

inline MediaPhase media_phase(const std::string &state) {
  if (state == "playing") return MediaPhase::Playing;
  if (state == "paused") return MediaPhase::Paused;
  if (state == "buffering") return MediaPhase::Buffering;
  if (state == "idle" || state == "on") return MediaPhase::Idle;
  if (state == "off" || state == "standby") return MediaPhase::Off;
  return MediaPhase::Unavailable;  // unavailable, unknown, not yet received
}

// Something is loaded: the transport and the track lines mean something.
inline bool media_has_track(MediaPhase p) {
  return p == MediaPhase::Playing || p == MediaPhase::Paused || p == MediaPhase::Buffering;
}

inline const char *media_caption(MediaPhase p) {
  switch (p) {
    case MediaPhase::Playing: return "PLAYING";
    case MediaPhase::Paused: return "PAUSED";
    case MediaPhase::Buffering: return "LOADING";
    case MediaPhase::Idle: return "IDLE";
    case MediaPhase::Off: return "OFF";
    case MediaPhase::Unavailable: break;
  }
  return "UNAVAILABLE";
}

// A float attribute in seconds ("187.0", "187") -> whole seconds; -1 if empty
// or not a number.
inline int64_t parse_seconds(const std::string &s) {
  if (s.empty()) return -1;
  char *end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (end == s.c_str() || !std::isfinite(v) || v < 0) return -1;
  return static_cast<int64_t>(v);
}

struct MediaProgress {
  int64_t position_s = -1;  // -1 = unknown
  int64_t duration_s = -1;  // -1 = unknown, or a live stream

  // 0..1000 of the track played, for the bar; 0 when unknown.
  int permille() const {
    if (position_s < 0 || duration_s <= 0) return 0;
    const int64_t v = position_s * 1000 / duration_s;
    return static_cast<int>(v > 1000 ? 1000 : v);
  }
};

// Where the track is now. While playing, the position runs on from when HA
// last reported it; `now_s` is the panel's Unix time, or -1 before its clock
// has synced, in which case the last reported position stands.
inline MediaProgress media_progress(MediaPhase phase, const std::string &position,
                                    const std::string &updated_at,
                                    const std::string &duration, int64_t now_s) {
  MediaProgress p;
  p.duration_s = parse_seconds(duration);
  if (!media_has_track(phase)) return p;
  p.position_s = parse_seconds(position);
  if (p.position_s < 0) return p;
  if (phase == MediaPhase::Playing && now_s >= 0) {
    const int64_t at = parse_iso8601(updated_at);
    if (at >= 0 && now_s > at) p.position_s += now_s - at;
  }
  if (p.duration_s > 0 && p.position_s > p.duration_s) p.position_s = p.duration_s;
  return p;
}

// "1:23 / 4:56", or just "1:23" for a stream with no duration; "" if unknown.
inline std::string format_media_progress(const MediaProgress &p) {
  if (p.position_s < 0) return "";
  std::string s = format_countdown(p.position_s);
  if (p.duration_s > 0) s += " / " + format_countdown(p.duration_s);
  return s;
}

// HA's volume_level (0..1) -> 0..100; -1 if missing.
inline int volume_pct(const std::string &level) {
  if (level.empty()) return -1;
  char *end = nullptr;
  const double v = std::strtod(level.c_str(), &end);
  if (end == level.c_str() || !std::isfinite(v)) return -1;
  const long pct = std::lround(v * 100.0);
  return static_cast<int>(pct < 0 ? 0 : (pct > 100 ? 100 : pct));
}

// 0..100 -> the string media_player.volume_set takes: "0.36", "1.00".
inline std::string volume_level_arg(int pct) {
  pct = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%d.%02d", pct / 100, pct % 100);
  return buf;
}

}  // namespace tilehaus
