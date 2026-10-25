#include <cassert>
#include <string>
#include "media_state.h"

int main() {
  using namespace tilehaus;

  // States, including the ones players report besides playing and paused.
  assert(media_phase("playing") == MediaPhase::Playing);
  assert(media_phase("paused") == MediaPhase::Paused);
  assert(media_phase("buffering") == MediaPhase::Buffering);
  assert(media_phase("idle") == MediaPhase::Idle);
  assert(media_phase("on") == MediaPhase::Idle);
  assert(media_phase("off") == MediaPhase::Off);
  assert(media_phase("standby") == MediaPhase::Off);
  assert(media_phase("unavailable") == MediaPhase::Unavailable);
  assert(media_phase("") == MediaPhase::Unavailable);
  assert(media_has_track(MediaPhase::Paused));
  assert(!media_has_track(MediaPhase::Idle));
  assert(std::string(media_caption(MediaPhase::Buffering)) == "LOADING");

  assert(parse_seconds("187.6") == 187);
  assert(parse_seconds("0") == 0);
  assert(parse_seconds("") == -1);
  assert(parse_seconds("None") == -1);
  assert(parse_seconds("-3") == -1);

  // 2026-10-24T14:00:00Z
  const int64_t t0 = 1792850400;
  const std::string at = "2026-10-24T14:00:00+00:00";

  // Playing: the position runs on from when HA reported it.
  {
    MediaProgress p = media_progress(MediaPhase::Playing, "60.0", at, "240.0", t0 + 30);
    assert(p.position_s == 90 && p.duration_s == 240);
    assert(p.permille() == 375);
    assert(format_media_progress(p) == "1:30 / 4:00");
  }
  // ... but never past the end of the track.
  {
    MediaProgress p = media_progress(MediaPhase::Playing, "200", at, "240", t0 + 600);
    assert(p.position_s == 240 && p.permille() == 1000);
  }
  // Paused holds the reported position however late it is.
  {
    MediaProgress p = media_progress(MediaPhase::Paused, "60", at, "240", t0 + 600);
    assert(p.position_s == 60);
  }
  // No synced clock yet: the reported position stands.
  {
    MediaProgress p = media_progress(MediaPhase::Playing, "60", at, "240", -1);
    assert(p.position_s == 60);
  }
  // A stream has no duration: elapsed time only, and an empty bar.
  {
    MediaProgress p = media_progress(MediaPhase::Playing, "65", at, "", t0);
    assert(p.duration_s == -1 && p.permille() == 0);
    assert(format_media_progress(p) == "1:05");
  }
  // Nothing loaded: no position, so no progress line.
  {
    MediaProgress p = media_progress(MediaPhase::Idle, "60", at, "240", t0);
    assert(p.position_s == -1);
    assert(format_media_progress(p).empty());
  }
  // Hour-long tracks.
  {
    MediaProgress p = media_progress(MediaPhase::Paused, "3725", at, "7200", t0);
    assert(format_media_progress(p) == "1:02:05 / 2:00:00");
  }

  // Volume, both ways.
  assert(volume_pct("0.36000001430511475") == 36);
  assert(volume_pct("1.0") == 100);
  assert(volume_pct("1.7") == 100);
  assert(volume_pct("") == -1);
  assert(volume_level_arg(36) == "0.36");
  assert(volume_level_arg(5) == "0.05");
  assert(volume_level_arg(100) == "1.00");
  assert(volume_level_arg(140) == "1.00");
  return 0;
}
