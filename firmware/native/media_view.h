#pragma once
#include "lvgl.h"
#include <algorithm>
#include <memory>
#include <string>
#include "card.h"
#include "card_style.h"
#include "clock.h"
#include "ha.h"
#include "ha_hold.h"
#include "media_art.h"
#include "media_state.h"
#include "overlay.h"
#include "theme.h"

namespace tilehaus {

// What a Media card knows about its player, kept by the card and read by its
// full view.
struct MediaInfo {
  std::string state, title, artist, album, app;
  std::string position, updated_at, duration;
  std::string picture;  // entity_picture
  std::string shuffle, repeat;
  int volume = -1;      // 0..100, -1 = not reported

  MediaPhase phase() const { return media_phase(state); }
  // HA's booleans reach the panel as "on" / "off".
  bool shuffled() const { return shuffle == "on" || shuffle == "true"; }
  bool track() const { return media_has_track(phase()); }
  // The title line: the track, else the app playing it; nothing when idle.
  // HA keeps an idle player's app name and never says when a track attribute
  // goes away, so nothing from the last session is shown.
  std::string title_line() const { return track() ? (title.empty() ? app : title) : std::string(); }
};

// The Media card's full-screen now-playing view, opened from the tile's
// corner button: the cover on the left; the track, artist and album on the
// right, then the progress, the transport (shuffle, previous, play / pause,
// next, repeat) and the volume. Built on first open; it holds no HA
// subscriptions of its own, the card hands it every update.
struct MediaView {
  Overlay overlay_;
  // Cleared when the view goes: a cover download finishing later must not
  // call back into it (a live rebuild destroys the card, card_host.h).
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
  MediaView() = default;
  MediaView(const MediaView &) = delete;
  ~MediaView() { *alive_ = false; }
  const MediaInfo *info_ = nullptr;
  std::string entity_, name_;
  CardFonts fonts_{};
  bool built_ = false;

  lv_obj_t *art_ = nullptr;     // the cover, over the placeholder well
  lv_obj_t *caption_ = nullptr;
  lv_obj_t *title_ = nullptr;
  lv_obj_t *artist_ = nullptr;
  lv_obj_t *album_ = nullptr;
  lv_obj_t *bar_ = nullptr;
  lv_obj_t *elapsed_ = nullptr;
  lv_obj_t *total_ = nullptr;
  lv_obj_t *play_btn_ = nullptr;
  lv_obj_t *play_lbl_ = nullptr;
  lv_obj_t *shuffle_lbl_ = nullptr;
  lv_obj_t *repeat_lbl_ = nullptr;
  lv_obj_t *volume_ = nullptr;
  HaHold vol_hold_;
  lv_subject_t vol_value_{};
  int art_px_ = 0;
  std::string art_url_;
  ArtRef art_ref_;

  void init(const MediaInfo *info, const std::string &entity, const std::string &name,
            const CardFonts &fonts) {
    info_ = info;
    entity_ = entity;
    name_ = name.empty() ? std::string("Now Playing") : name;
    fonts_ = fonts;
  }

  bool visible() const { return built_ && active_overlay() == &overlay_; }

  void open() {
    if (!built_) build();
    render();
    overlay_.show();
    render_art();  // after show: only a visible view fetches its cover
  }

  // Everything but the position (render_progress).
  void render() {
    if (!built_ || !info_) return;
    const MediaInfo &m = *info_;
    const MediaPhase phase = m.phase();
    const bool track = m.track();
    lv_label_set_text(caption_, media_caption(phase));
    lv_label_set_text(title_, m.title_line().c_str());
    lv_label_set_text(artist_, track ? m.artist.c_str() : "");
    lv_label_set_text(album_, track ? m.album.c_str() : "");
    lv_label_set_text(play_lbl_, phase == MediaPhase::Playing ? glyph_pause() : glyph_play());
    lv_label_set_text(shuffle_lbl_, m.shuffled() ? glyph_shuffle() : glyph_shuffle_off());
    lv_label_set_text(repeat_lbl_, m.repeat == "one"   ? glyph_repeat_one()
                                   : m.repeat == "all" ? glyph_repeat()
                                                       : glyph_repeat_off());
    const uint32_t accent = tile_on_color();
    // The accent can change at runtime (restyle), so it is set on each pass.
    lv_obj_set_style_bg_color(play_btn_, lv_color_hex(accent), 0);
    lv_obj_set_style_bg_color(bar_, lv_color_hex(accent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_, lv_color_hex(accent), LV_PART_INDICATOR);
    const uint32_t play_ink = needs_dark_ink(accent) ? kTileInkOnLight : kTileInk;
    lv_obj_set_style_text_color(play_lbl_, lv_color_hex(play_ink), 0);
    lv_obj_set_style_text_color(shuffle_lbl_, lv_color_hex(m.shuffled() ? accent : kTileInkDim), 0);
    lv_obj_set_style_text_color(repeat_lbl_,
                                lv_color_hex(m.repeat == "one" || m.repeat == "all" ? accent : kTileInkDim), 0);
    if (m.volume >= 0) {
      const int v = m.volume;
      lv_subject_t *subj = &vol_value_;
      vol_hold_.gate([subj, v]() { lv_subject_set_int(subj, v); });
    }
    if (track) lv_obj_remove_flag(bar_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(bar_, LV_OBJ_FLAG_HIDDEN);
    render_progress();
    if (visible()) render_art();
  }

  void render_progress() {
    if (!built_ || !info_) return;
    const MediaInfo &m = *info_;
    const MediaProgress p = media_progress(m.phase(), m.position, m.updated_at, m.duration, now_s());
    lv_bar_set_value(bar_, p.permille(), LV_ANIM_OFF);
    lv_label_set_text(elapsed_, p.position_s >= 0 ? format_countdown(p.position_s).c_str() : "");
    lv_label_set_text(total_, p.position_s >= 0 && p.duration_s > 0
                                  ? format_countdown(p.duration_s).c_str() : "");
  }

 private:
  static const char *glyph_play() { return "\U000F040A"; }         // mdi-play
  static const char *glyph_pause() { return "\U000F03E4"; }        // mdi-pause
  static const char *glyph_prev() { return "\U000F04AE"; }         // mdi-skip-previous
  static const char *glyph_next() { return "\U000F04AD"; }         // mdi-skip-next
  static const char *glyph_shuffle() { return "\U000F049D"; }      // mdi-shuffle
  static const char *glyph_shuffle_off() { return "\U000F049E"; }  // mdi-shuffle-disabled
  static const char *glyph_repeat() { return "\U000F0456"; }       // mdi-repeat
  static const char *glyph_repeat_off() { return "\U000F0457"; }   // mdi-repeat-off
  static const char *glyph_repeat_one() { return "\U000F0458"; }   // mdi-repeat-once
  static const char *glyph_album() { return "\U000F0025"; }        // mdi-album
  static const char *glyph_volume() { return "\U000F057E"; }       // mdi-volume-high

  static int64_t now_s() {
    if (app_clock() == nullptr) return -1;
    const esphome::ESPTime t = app_clock()->now();
    return t.is_valid() ? static_cast<int64_t>(t.timestamp) : -1;
  }

  static int line_h(const lv_font_t *f, int fallback) {
    return f ? static_cast<int>(lv_font_get_line_height(f)) : fallback;
  }

  // Text on the page: ink (or muted ink) that night mode flips with the page.
  static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, int w, uint32_t color, int lines) {
    lv_obj_t *l = lv_label_create(parent);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_width(l, w);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_height(l, line_h(font, 24) * lines);
    lv_label_set_text(l, "");
    return l;
  }

  // A square transport button, charcoal like the other modals' controls.
  lv_obj_t *button(lv_obj_t *parent, int size, int x, int y, const char *glyph, lv_event_cb_t cb,
                   lv_obj_t **glyph_out) {
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, size, size);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_bg_color(b, lv_color_hex(kControlBg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    style_press_dip(b);
    lv_obj_t *g = lv_label_create(b);
    if (fonts_.icon) lv_obj_set_style_text_font(g, fonts_.icon, 0);
    lv_obj_set_style_text_color(g, lv_color_hex(kTileInk), 0);
    lv_label_set_text(g, glyph);
    lv_obj_center(g);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, this);
    if (glyph_out) *glyph_out = g;
    return b;
  }

  static MediaView *self_of(lv_event_t *e) {
    return static_cast<MediaView *>(lv_event_get_user_data(e));
  }

  void build() {
    built_ = true;
    overlay_.build(name_, fonts_);
    overlay_.on_close_ = [this]() { close(); };
    overlay_.stays_open_ = true;  // a now-playing screen is meant to be left up
    lv_obj_t *c = overlay_.content();
    const int cw = overlay_.content_w(), ch = overlay_.content_h();
    constexpr int kTop = 20;  // air under the header

    // The cover: a square as tall as the body allows, over a well with the
    // album glyph that shows while it loads or when there is none.
    art_px_ = std::min(ch - kTop, cw / 2);
    lv_obj_t *well = lv_obj_create(c);
    lv_obj_remove_style_all(well);
    lv_obj_set_size(well, art_px_, art_px_);
    lv_obj_set_pos(well, 0, kTop);
    lv_obj_set_style_bg_color(well, lv_color_hex(kModalWell), 0);
    lv_obj_set_style_bg_opa(well, LV_OPA_COVER, 0);
    lv_obj_remove_flag(well, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *ph = lv_label_create(well);
    if (fonts_.icon) lv_obj_set_style_text_font(ph, fonts_.icon, 0);
    lv_obj_set_style_text_color(ph, lv_color_hex(kTileInkFaint), 0);
    lv_label_set_text(ph, glyph_album());
    lv_obj_set_style_transform_pivot_x(ph, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(ph, lv_pct(50), 0);
    lv_obj_set_style_transform_scale(ph, 768, 0);  // 3x the 46px glyph
    lv_obj_center(ph);
    art_ = lv_image_create(well);
    lv_obj_set_size(art_, art_px_, art_px_);
    lv_obj_set_pos(art_, 0, 0);
    lv_obj_add_flag(art_, LV_OBJ_FLAG_HIDDEN);

    // The text column, from the top: caption, title, artist, album.
    const int x = art_px_ + 40, w = cw - x;
    const lv_font_t *tfont = fonts_.title ? fonts_.title : fonts_.medium;
    caption_ = text(c, fonts_.caption, w, kInkMuted, 1);
    lv_obj_set_style_text_letter_space(caption_, 2, 0);
    lv_obj_set_pos(caption_, x, kTop);
    int y = kTop + line_h(fonts_.caption, 17) + 10;
    title_ = text(c, tfont, w, kInk, 2);
    lv_label_set_long_mode(title_, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(title_, x, y);
    y += line_h(tfont, 56) * 2 + 4;
    artist_ = text(c, fonts_.medium, w, kInk, 1);
    lv_obj_set_pos(artist_, x, y);
    y += line_h(fonts_.medium, 42) + 4;
    album_ = text(c, fonts_.body, w, kInkMuted, 1);
    lv_obj_set_pos(album_, x, y);

    // From the bottom: volume, the transport, then the progress and times.
    constexpr int kVolH = 48, kBtn = 76;
    const int vol_y = kTop + art_px_ - kVolH;
    lv_obj_t *vi = lv_label_create(c);
    if (fonts_.icon) lv_obj_set_style_text_font(vi, fonts_.icon, 0);
    lv_obj_set_style_text_color(vi, lv_color_hex(kInk), 0);
    lv_label_set_text(vi, glyph_volume());
    lv_obj_set_pos(vi, x, vol_y + (kVolH - line_h(fonts_.icon, 46)) / 2);
    volume_ = lv_slider_create(c);
    lv_obj_set_size(volume_, w - 64, kVolH);
    lv_obj_set_pos(volume_, x + 64, vol_y);
    lv_slider_set_range(volume_, 0, 100);
    lv_obj_set_style_bg_color(volume_, lv_color_hex(kModalWell), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(volume_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(volume_, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(volume_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(volume_, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(volume_, 0, LV_PART_MAIN);
    lv_subject_init_int(&vol_value_, 0);
    lv_slider_bind_value(volume_, &vol_value_);
    vol_hold_.attach(volume_);
    lv_obj_add_event_cb(volume_, [](lv_event_t *e) {
      MediaView *self = self_of(e);
      ha_call_kv("media_player.volume_set", self->entity_, "volume_level",
                 volume_level_arg(static_cast<int>(lv_slider_get_value(self->volume_))));
    }, LV_EVENT_RELEASED, this);

    const int btn_y = vol_y - 24 - kBtn;
    const int gap = (w - 5 * kBtn) / 4;
    auto bx = [&](int i) { return x + i * (kBtn + gap); };
    button(c, kBtn, bx(0), btn_y, glyph_shuffle_off(), [](lv_event_t *e) {
      MediaView *self = self_of(e);
      ha_call_kv("media_player.shuffle_set", self->entity_, "shuffle",
                 self->info_->shuffled() ? "false" : "true");
    }, &shuffle_lbl_);
    button(c, kBtn, bx(1), btn_y, glyph_prev(), [](lv_event_t *e) {
      ha_call("media_player.media_previous_track", self_of(e)->entity_);
    }, nullptr);
    play_btn_ = button(c, kBtn, bx(2), btn_y, glyph_play(), [](lv_event_t *e) {
      MediaView *self = self_of(e);
      ha_call(self->info_->phase() == MediaPhase::Playing ? "media_player.media_pause"
                                                          : "media_player.media_play",
              self->entity_);
    }, &play_lbl_);
    button(c, kBtn, bx(3), btn_y, glyph_next(), [](lv_event_t *e) {
      ha_call("media_player.media_next_track", self_of(e)->entity_);
    }, nullptr);
    button(c, kBtn, bx(4), btn_y, glyph_repeat_off(), [](lv_event_t *e) {
      MediaView *self = self_of(e);
      ha_call_kv("media_player.repeat_set", self->entity_, "repeat",
                 media_repeat_next(self->info_->repeat));
    }, &repeat_lbl_);

    const int cap_h = line_h(fonts_.caption, 17);
    const int times_y = btn_y - 18 - cap_h;
    elapsed_ = text(c, fonts_.caption, w / 2, kInk, 1);
    lv_obj_set_pos(elapsed_, x, times_y);
    total_ = text(c, fonts_.caption, w / 2, kInk, 1);
    lv_obj_set_style_text_align(total_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(total_, x + w - w / 2, times_y);
    bar_ = lv_bar_create(c);
    lv_obj_set_size(bar_, w, 8);
    lv_obj_set_pos(bar_, x, times_y - 10 - 8);
    lv_bar_set_range(bar_, 0, 1000);
    lv_obj_remove_flag(bar_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(bar_, lv_color_hex(kModalWell), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_, LV_OPA_COVER, LV_PART_INDICATOR);
  }

  void close() {
    overlay_.hide();
    // The full-size cover is half a megabyte; let it go (the art cache keeps
    // the latest few anyway).
    if (art_) {
      lv_obj_add_flag(art_, LV_OBJ_FLAG_HIDDEN);
      lv_image_set_src(art_, nullptr);
    }
    art_ref_.reset();
    art_url_.clear();
  }

  // Show the cover for the current track, fetching it if need be.
  void render_art() {
    const MediaInfo &m = *info_;
    const std::string url =
        m.track() ? media_art_url(m.picture, media_art().ha_base(), art_px_) : std::string();
    if (url == art_url_) return;
    art_url_ = url;
    art_ref_.reset();
    lv_obj_add_flag(art_, LV_OBJ_FLAG_HIDDEN);
    lv_image_set_src(art_, nullptr);
    if (url.empty()) return;
    media_art().request(url, art_px_, [this, alive = alive_, url](ArtRef img) {
      if (!*alive) return;
      if (url != art_url_ || !img) return;  // the track moved on meanwhile
      art_ref_ = img;
      lv_image_set_src(art_, &img->dsc);
      lv_obj_remove_flag(art_, LV_OBJ_FLAG_HIDDEN);
    });
  }
};

}  // namespace tilehaus
