#pragma once
#include "lvgl.h"
#include <algorithm>
#include <cctype>
#include <string>
#include "card.h"
#include "card_style.h"
#include "clock.h"
#include "ha.h"
#include "ha_hold.h"
#include "media_state.h"
#include "slider_hold.h"
#include "theme.h"

namespace tilehaus {

// A media_player as now-playing (type 29): the track, the artist, how far in,
// and the transport. Any player works, Music Assistant's included. Text only
// for now; artwork is for later. The tile tints while playing, and a tap on it
// plays or pauses.
//
// Three layouts, by the tile's content size:
//  - small (a 2x2): state caption, title, artist, and the name at the bottom;
//  - wide (a 4x2): the name joins the caption, and a bottom row has the
//    progress on the left and previous / play-pause / next on the right;
//  - large (3x4, 4x4): a bigger two-line title, a full-width progress line,
//    larger transport buttons, and a volume slider that needs a hold like the
//    tile sliders.
//
// HA reports the position only when playback changes, so while playing a 1s
// LVGL timer recounts it from media_position_updated_at (media_state.h).
struct MediaCard : Card {
  enum class Layout : uint8_t { Small, Wide, Large };

  lv_obj_t *cell_ = nullptr;
  lv_obj_t *caption_ = nullptr;
  lv_obj_t *title_ = nullptr;
  lv_obj_t *artist_ = nullptr;
  lv_obj_t *icon_ = nullptr;    // small layout, nothing loaded: the speaker
  lv_obj_t *bar_ = nullptr;
  lv_obj_t *time_ = nullptr;
  lv_obj_t *play_lbl_ = nullptr;
  lv_obj_t *volume_ = nullptr;
  lv_obj_t *vol_icon_ = nullptr;
  SliderHold vol_press_;
  HaHold vol_hold_;
  lv_subject_t vol_value_{};
  CardFonts fonts_{};
  Layout layout_ = Layout::Small;
  std::string entity_, name_;
  std::string state_, title_text_, artist_text_, position_, updated_at_, duration_, app_;
  uint32_t on_color_ = kTileOnBuiltin;
  uint32_t off_color_ = kTileBg;
  int32_t cfg_active_color_ = -1;
  MediaPhase phase_ = MediaPhase::Unavailable;

  TileSpan default_size() const override { return {4, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg, const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    name_ = cfg.hide_label ? std::string() : cfg.title;
    cfg_active_color_ = cfg.active_color;
    on_color_ = resolve_color(cfg_active_color_, tile_on_color());
    off_color_ = resolve_color(cfg.inactive_color, kTileBg);

    const TileMetrics m = tile_metrics();
    const int cw = m.px_w > 0 ? m.px_w - 2 * kTileInset : 124;
    const int ch = m.px_h > 0 ? m.px_h - 2 * kTileInset : 124;
    layout_ = cw >= 200 && ch >= 240 ? Layout::Large
            : cw >= 200 && ch >= 110 ? Layout::Wide
            : Layout::Small;

    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    if (tile_compact()) {  // a 1x1: just the play state, centred
      icon_ = add_icon(cell, fonts.icon, glyph_play());
      return;
    }

    caption_ = label(cell, fonts.caption, cw);
    lv_obj_set_style_text_letter_space(caption_, 2, 0);
    lv_obj_set_style_text_opa(caption_, LV_OPA_70, 0);
    lv_obj_align(caption_, LV_ALIGN_TOP_LEFT, 0, 0);

    const bool large = layout_ == Layout::Large;
    const lv_font_t *title_font = large ? fonts.medium : tile_body_font(fonts);
    const lv_font_t *artist_font = large ? fonts.body : fonts.body_tight;
    const int cap_h = font_h(fonts.caption, 17);
    const int title_h = font_h(title_font, 28) * (large ? 2 : 1);
    const int artist_y = cap_h + 4 + title_h;

    title_ = label(cell, title_font, cw);
    lv_label_set_long_mode(title_, LV_LABEL_LONG_DOT);
    lv_obj_set_height(title_, title_h);
    lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 0, cap_h + 4);

    artist_ = label(cell, artist_font, cw);
    lv_label_set_long_mode(artist_, LV_LABEL_LONG_DOT);
    lv_obj_set_height(artist_, font_h(artist_font, 22));
    lv_obj_set_style_text_opa(artist_, LV_OPA_70, 0);
    lv_obj_align(artist_, LV_ALIGN_TOP_LEFT, 0, artist_y);

    if (layout_ == Layout::Small) {
      icon_ = add_icon(cell, fonts.icon, glyph_speaker(), LV_ALIGN_LEFT_MID, 0, 0);
      add_name(cell, fonts, name_, name_.empty());
      return;
    }

    // Offsets are fixed arithmetic from the tile's edges rather than
    // lv_obj_align_to(), which reads coordinates that are not laid out yet.
    if (layout_ == Layout::Wide) {
      // Bottom row: progress on the left, the transport on the right.
      constexpr int kBtn = 48, kGap = 2;
      button(cell, kBtn, glyph_next(), "media_player.media_next_track", LV_ALIGN_BOTTOM_RIGHT, 0, 0);
      play_lbl_ = button(cell, kBtn, glyph_play(), nullptr, LV_ALIGN_BOTTOM_RIGHT, -(kBtn + kGap), 0);
      button(cell, kBtn, glyph_prev(), "media_player.media_previous_track",
             LV_ALIGN_BOTTOM_RIGHT, -2 * (kBtn + kGap), 0);
      const int left_w = cw - 3 * kBtn - 2 * kGap - 16;
      const int bar_y = -(kBtn - 4) / 2;
      bar_ = progress_bar(cell, left_w);
      lv_obj_align(bar_, LV_ALIGN_BOTTOM_LEFT, 0, bar_y);
      time_ = label(cell, fonts.caption, left_w);
      lv_obj_align(time_, LV_ALIGN_BOTTOM_LEFT, 0, bar_y - 4 - 4);
      return;
    }

    // Large, from the bottom up: volume, the transport, then the progress bar
    // with the time under it.
    constexpr int kBtn = 56, kVolH = 46, kGap = 8;
    vol_icon_ = label(cell, fonts.icon, LV_SIZE_CONTENT);
    lv_label_set_text(vol_icon_, glyph_volume());
    lv_obj_align(vol_icon_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    volume_ = lv_slider_create(cell);
    vol_press_.attach(volume_);  // first: a brush of the bar changes nothing
    lv_obj_set_size(volume_, cw - 60, 8);
    lv_obj_align(volume_, LV_ALIGN_BOTTOM_RIGHT, 0, -(kVolH - 8) / 2);
    lv_obj_set_ext_click_area(volume_, 18);
    lv_slider_set_range(volume_, 0, 100);
    lv_obj_set_style_radius(volume_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(volume_, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(volume_, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(volume_, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(volume_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(volume_, 0, LV_PART_KNOB);
    lv_subject_init_int(&vol_value_, 0);
    lv_slider_bind_value(volume_, &vol_value_);

    const int btn_y = -(kVolH + kGap);
    play_lbl_ = button(cell, kBtn, glyph_play(), nullptr, LV_ALIGN_BOTTOM_MID, 0, btn_y);
    button(cell, kBtn, glyph_prev(), "media_player.media_previous_track",
           LV_ALIGN_BOTTOM_MID, -(kBtn + 24), btn_y);
    button(cell, kBtn, glyph_next(), "media_player.media_next_track",
           LV_ALIGN_BOTTOM_MID, kBtn + 24, btn_y);

    const int time_y = btn_y - kBtn - 10;
    time_ = label(cell, fonts.caption, cw);
    lv_obj_align(time_, LV_ALIGN_BOTTOM_LEFT, 0, time_y);
    bar_ = progress_bar(cell, cw);
    lv_obj_align(bar_, LV_ALIGN_BOTTOM_LEFT, 0, time_y - cap_h - 6);
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    auto sub = [this](const char *attr, std::string *into) {
      ha_subscribe(entity_, attr, [this, into](const std::string &s) { *into = s; render(); });
    };
    sub(nullptr, &state_);
    sub("media_title", &title_text_);
    sub("media_artist", &artist_text_);
    sub("media_position", &position_);
    sub("media_position_updated_at", &updated_at_);
    sub("media_duration", &duration_);
    sub("app_name", &app_);

    if (volume_) {
      vol_hold_.attach(volume_);
      vol_press_.on_cancel = [this]() { vol_hold_.release(); };
      lv_subject_t *subj = &vol_value_;
      ha_subscribe(entity_, "volume_level", [this, subj](const std::string &s) {
        const int v = volume_pct(s);
        if (v >= 0) vol_hold_.gate([subj, v]() { lv_subject_set_int(subj, v); });
      });
      lv_obj_add_event_cb(volume_, [](lv_event_t *e) {
        auto *self = static_cast<MediaCard *>(lv_event_get_user_data(e));
        ha_call_kv("media_player.volume_set", self->entity_, "volume_level",
                   volume_level_arg(static_cast<int>(lv_slider_get_value(self->volume_))));
      }, LV_EVENT_RELEASED, this);
    }

    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      static_cast<MediaCard *>(lv_event_get_user_data(e))->play_pause();
    }, LV_EVENT_CLICKED, this);

    lv_timer_create([](lv_timer_t *t) {
      auto *self = static_cast<MediaCard *>(lv_timer_get_user_data(t));
      if (self->phase_ == MediaPhase::Playing) self->render_progress();
    }, 1000, this);
  }

  void restyle() override {
    on_color_ = resolve_color(cfg_active_color_, tile_on_color());
    render();
  }

 private:
  static const char *glyph_play() { return "\U000F040A"; }     // mdi-play
  static const char *glyph_pause() { return "\U000F03E4"; }    // mdi-pause
  static const char *glyph_prev() { return "\U000F04AE"; }     // mdi-skip-previous
  static const char *glyph_next() { return "\U000F04AD"; }     // mdi-skip-next
  static const char *glyph_speaker() { return "\U000F04C3"; }  // mdi-speaker
  static const char *glyph_volume() { return "\U000F057E"; }   // mdi-volume-high

  static int font_h(const lv_font_t *f, int fallback) {
    return f ? static_cast<int>(lv_font_get_line_height(f)) : fallback;
  }

  static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, int w) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_color(l, lv_color_hex(kTileInk), 0);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_width(l, w);
    lv_label_set_text(l, "");
    return l;
  }

  static lv_obj_t *progress_bar(lv_obj_t *parent, int w) {
    lv_obj_t *b = lv_bar_create(parent);
    lv_obj_set_size(b, w, 4);
    lv_bar_set_range(b, 0, 1000);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(b, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(b, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(b, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_INDICATOR);
    return b;
  }

  // A transport button: a bare glyph that darkens under the finger. A plain
  // background change, not the tile's press dip: cheap to redraw on every
  // frame of a quick double tap. `service` = nullptr makes it play / pause.
  // Returns the glyph label, so play / pause can swap it.
  lv_obj_t *button(lv_obj_t *parent, int size, const char *glyph, const char *service,
                   lv_align_t align, int dx, int dy) {
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, size, size);
    lv_obj_align(b, align, dx, dy);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(b, lv_color_hex(kBlack), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_t *g = lv_label_create(b);
    lv_obj_set_style_text_color(g, lv_color_hex(kTileInk), 0);
    if (fonts_.icon) lv_obj_set_style_text_font(g, fonts_.icon, 0);
    lv_label_set_text(g, glyph);
    lv_obj_center(g);
    if (service) {
      lv_obj_set_user_data(b, const_cast<char *>(service));
      lv_obj_add_event_cb(b, [](lv_event_t *e) {
        auto *self = static_cast<MediaCard *>(lv_event_get_user_data(e));
        auto *btn = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
        ha_call(static_cast<const char *>(lv_obj_get_user_data(btn)), self->entity_);
      }, LV_EVENT_CLICKED, this);
    } else {
      lv_obj_add_event_cb(b, [](lv_event_t *e) {
        static_cast<MediaCard *>(lv_event_get_user_data(e))->play_pause();
      }, LV_EVENT_CLICKED, this);
    }
    return g;
  }

  void play_pause() {
    if (entity_.empty()) return;
    ha_call(phase_ == MediaPhase::Playing ? "media_player.media_pause"
                                          : "media_player.media_play", entity_);
  }

  static int64_t now_s() {
    if (app_clock() == nullptr) return -1;
    const esphome::ESPTime t = app_clock()->now();
    return t.is_valid() ? static_cast<int64_t>(t.timestamp) : -1;
  }

  // The caption: "PLAYING", or with the name on the layouts that have no name
  // line ("PLAYING · OFFICE").
  std::string caption_text() const {
    std::string s = media_caption(phase_);
    if (layout_ != Layout::Small && !name_.empty()) {
      std::string up = name_;
      std::transform(up.begin(), up.end(), up.begin(),
                     [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
      s += " \xC2\xB7 " + up;  // middle dot
    }
    return s;
  }

  void render_progress() {
    if (!bar_ && !time_) return;
    const MediaProgress p = media_progress(phase_, position_, updated_at_, duration_, now_s());
    if (bar_) lv_bar_set_value(bar_, p.permille(), LV_ANIM_OFF);
    if (time_) lv_label_set_text(time_, format_media_progress(p).c_str());
  }

  void render() {
    if (cell_ == nullptr) return;
    phase_ = media_phase(state_);
    const bool playing = phase_ == MediaPhase::Playing;
    const bool track = media_has_track(phase_);
    const uint32_t fill = playing ? on_color_ : off_color_;
    paint_cell(cell_, fill);
    const uint32_t ink = needs_dark_ink(fill) ? kTileInkOnLight : kTileInk;

    if (tile_compact()) {
      set_icon_glyph(icon_, playing ? glyph_pause() : glyph_play());
      return;
    }

    lv_label_set_text(caption_, caption_text().c_str());
    // Nothing loaded: the tile rests on its state and name. HA keeps an idle
    // player's app name, and never tells the panel when a track attribute is
    // dropped, so nothing from the last session is shown.
    const std::string title = track ? (title_text_.empty() ? app_ : title_text_)
                                    : std::string();
    lv_label_set_text(title_, title.c_str());
    lv_label_set_text(artist_, track ? artist_text_.c_str() : "");
    if (icon_) {  // small layout: the speaker stands in for an empty title
      if (title.empty()) lv_obj_remove_flag(icon_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(icon_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_t *sh = static_cast<lv_obj_t *>(lv_obj_get_user_data(icon_));
      if (sh) {
        if (title.empty()) lv_obj_remove_flag(sh, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(sh, LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (play_lbl_) lv_label_set_text(play_lbl_, playing ? glyph_pause() : glyph_play());

    // Bars and the volume slider are not labels, so the ink is set here.
    for (lv_obj_t *b : {bar_, volume_}) {
      if (!b) continue;
      lv_obj_set_style_bg_color(b, lv_color_hex(ink), LV_PART_MAIN);
      lv_obj_set_style_bg_color(b, lv_color_hex(ink), LV_PART_INDICATOR);
    }
    if (bar_) {
      if (track) lv_obj_remove_flag(bar_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(bar_, LV_OBJ_FLAG_HIDDEN);
    }
    render_progress();
  }
};

}  // namespace tilehaus
