#pragma once
#include "lvgl.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "driver/jpeg_decode.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "media_art_scale.h"
// lv_image_cache_drop is not in lvgl.h's public set.
#include "src/misc/cache/instance/lv_image_cache.h"

namespace tilehaus {

// Cover art for the Media card and its full view: downloaded over HTTP(S),
// decoded by the P4's JPEG engine, and fitted to a square of the size asked
// for. The work runs on a task of its own, so the UI never waits on the
// network; finished images come back on the LVGL loop.
//
// A picture is one image request: (URL, size). The last few are kept, so
// opening the full view again, or two tiles on the same player, cost nothing.
// Only baseline JPEG decodes (the engine does not do progressive JPEG or PNG);
// anything else, a failed download, or a picture too big to decode leaves the
// card on its placeholder.

// A decoded cover, ready for lv_image_set_src(&dsc). Freed when the last
// holder lets go, which is always on the LVGL loop.
struct ArtImage {
  lv_image_dsc_t dsc{};
  uint16_t *pixels = nullptr;

  ArtImage(uint16_t *px, int side) : pixels(px) {
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    dsc.header.w = side;
    dsc.header.h = side;
    dsc.header.stride = side * 2;
    dsc.data_size = static_cast<uint32_t>(side) * side * 2;
    dsc.data = reinterpret_cast<const uint8_t *>(px);
  }
  ArtImage(const ArtImage &) = delete;
  ArtImage &operator=(const ArtImage &) = delete;
  ~ArtImage() {
    lv_image_cache_drop(&dsc);
    heap_caps_free(pixels);
  }
};
using ArtRef = std::shared_ptr<const ArtImage>;

class MediaArt {
 public:
  // Runs on the LVGL loop with the image, or nullptr if there is none.
  using Ready = std::function<void(ArtRef)>;

  // Where Home Assistant is, for pictures it serves itself (see media_art_url).
  void set_ha_base(const std::string &base) { ha_base_ = base; }
  const std::string &ha_base() const { return ha_base_; }

  // Ask for `url` at `px` square. A kept image answers at once.
  void request(const std::string &url, int px, Ready cb) {
    if (url.empty() || px <= 0) { cb(nullptr); return; }
    const std::string key = url + "#" + std::to_string(px);
    for (auto it = kept_.begin(); it != kept_.end(); ++it) {
      if (it->first != key) continue;
      kept_.splice(kept_.begin(), kept_, it);  // most recent first
      cb(it->second);
      return;
    }
    for (const std::string &f : failed_) {
      if (f == key) { cb(nullptr); return; }
    }
    auto w = waiting_.find(key);
    if (w != waiting_.end()) { w->second.push_back(std::move(cb)); return; }
    waiting_[key].push_back(std::move(cb));
    start();
    std::vector<std::string> dropped;
    {
      std::lock_guard<std::mutex> lock(mu_);
      jobs_.push_back({url, px, key});
      // Skipping through tracks queues pictures nobody shows any more; only
      // the newest few are worth fetching.
      while (jobs_.size() > kMaxJobs) {
        dropped.push_back(jobs_.front().key);
        jobs_.pop_front();
      }
    }
    xSemaphoreGive(wake_);
    for (const std::string &k : dropped) answer(k, nullptr);
  }

 private:
  static constexpr const char *kTag = "tilehaus.art";
  static constexpr size_t kMaxJobs = 3;
  static constexpr size_t kKeep = 4;            // decoded images kept
  static constexpr size_t kMaxBytes = 2 << 20;  // a JPEG bigger than this is refused
  static constexpr uint32_t kMaxPixels = 2048u * 2048u;

  struct Job { std::string url; int px; std::string key; };
  struct Done { std::string key; int px; uint16_t *pixels; };

  std::string ha_base_;
  std::list<std::pair<std::string, ArtRef>> kept_;
  std::list<std::string> failed_;
  std::map<std::string, std::vector<Ready>> waiting_;

  // Shared with the task, under mu_.
  std::mutex mu_;
  std::deque<Job> jobs_;
  std::deque<Done> done_;

  SemaphoreHandle_t wake_ = nullptr;
  jpeg_decoder_handle_t jpeg_ = nullptr;

  void start() {
    if (wake_) return;
    wake_ = xSemaphoreCreateBinary();
    // TLS needs the room. The task waits on wake_ when there is nothing to do.
    xTaskCreatePinnedToCore([](void *self) { static_cast<MediaArt *>(self)->run(); },
                            "media_art", 12 * 1024, this, 2, nullptr, 1);
    lv_timer_create([](lv_timer_t *t) {
      static_cast<MediaArt *>(lv_timer_get_user_data(t))->collect();
    }, 100, this);
  }

  // LVGL loop: hand finished images to whoever asked.
  void collect() {
    std::deque<Done> done;
    {
      std::lock_guard<std::mutex> lock(mu_);
      done.swap(done_);
    }
    for (Done &d : done) {
      ArtRef img;
      if (d.pixels) {
        img = std::make_shared<const ArtImage>(d.pixels, d.px);
        kept_.emplace_front(d.key, img);
        while (kept_.size() > kKeep) kept_.pop_back();
      } else {
        failed_.push_front(d.key);
        while (failed_.size() > 8) failed_.pop_back();
      }
      answer(d.key, img);
    }
  }

  void answer(const std::string &key, const ArtRef &img) {
    auto w = waiting_.find(key);
    if (w == waiting_.end()) return;
    std::vector<Ready> cbs = std::move(w->second);
    waiting_.erase(w);
    for (Ready &cb : cbs) cb(img);
  }

  // ---- the task -----------------------------------------------------------

  void run() {
    for (;;) {
      xSemaphoreTake(wake_, portMAX_DELAY);
      for (;;) {
        Job job;
        {
          std::lock_guard<std::mutex> lock(mu_);
          if (jobs_.empty()) break;
          job = std::move(jobs_.front());
          jobs_.pop_front();
        }
        uint16_t *px = fetch(job.url, job.px);
        std::lock_guard<std::mutex> lock(mu_);
        done_.push_back({job.key, job.px, px});
      }
    }
  }

  struct Body {
    uint8_t *data = nullptr;
    size_t len = 0, cap = 0;
    bool too_big = false;
  };

  static esp_err_t on_http(esp_http_client_event_t *e) {
    if (e->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
    // A redirect's own body arrives here too; keep only the picture's.
    if (esp_http_client_get_status_code(e->client) != 200) return ESP_OK;
    auto *b = static_cast<Body *>(e->user_data);
    if (b->too_big) return ESP_OK;
    const size_t need = b->len + e->data_len;
    if (need > kMaxBytes) { b->too_big = true; return ESP_OK; }
    if (need > b->cap) {
      size_t cap = b->cap ? b->cap : 64 * 1024;
      while (cap < need) cap *= 2;
      auto *grown = static_cast<uint8_t *>(heap_caps_realloc(b->data, cap, MALLOC_CAP_SPIRAM));
      if (!grown) { b->too_big = true; return ESP_OK; }
      b->data = grown;
      b->cap = cap;
    }
    std::memcpy(b->data + b->len, e->data, e->data_len);
    b->len = need;
    return ESP_OK;
  }

  uint16_t *fetch(const std::string &url, int px) {
    Body body;
    esp_http_client_config_t cfg{};
    cfg.url = url.c_str();
    cfg.timeout_ms = 10000;
    cfg.buffer_size = 4096;
    cfg.event_handler = on_http;
    cfg.user_data = &body;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.max_redirection_count = 3;
    esp_http_client_handle_t http = esp_http_client_init(&cfg);
    if (!http) return nullptr;
    const esp_err_t err = esp_http_client_perform(http);
    const int status = esp_http_client_get_status_code(http);
    esp_http_client_cleanup(http);
    uint16_t *out = nullptr;
    if (err != ESP_OK || status != 200) {
      ESP_LOGW(kTag, "download failed (%s, HTTP %d): %s", esp_err_to_name(err), status, url.c_str());
    } else if (body.too_big) {
      ESP_LOGW(kTag, "picture over %u bytes: %s", static_cast<unsigned>(kMaxBytes), url.c_str());
    } else {
      out = decode(body.data, body.len, px, url);
    }
    heap_caps_free(body.data);
    return out;
  }

  uint16_t *decode(const uint8_t *data, size_t len, int px, const std::string &url) {
    if (len < 4 || data[0] != 0xFF || data[1] != 0xD8) {
      ESP_LOGW(kTag, "not a JPEG: %s", url.c_str());
      return nullptr;
    }
    if (!jpeg_) {
      jpeg_decode_engine_cfg_t ecfg{};
      ecfg.timeout_ms = 500;
      if (jpeg_new_decoder_engine(&ecfg, &jpeg_) != ESP_OK) {
        jpeg_ = nullptr;
        ESP_LOGW(kTag, "no JPEG decoder");
        return nullptr;
      }
    }
    jpeg_decode_picture_info_t info{};
    if (jpeg_decoder_get_info(data, len, &info) != ESP_OK || info.width == 0 || info.height == 0 ||
        info.width * info.height > kMaxPixels) {
      ESP_LOGW(kTag, "can't decode %ux%u: %s", static_cast<unsigned>(info.width),
               static_cast<unsigned>(info.height), url.c_str());
      return nullptr;
    }
    // The engine writes whole blocks: rows and columns padded to them.
    const bool wide_mcu = info.sample_method == JPEG_DOWN_SAMPLING_YUV420 ||
                          info.sample_method == JPEG_DOWN_SAMPLING_YUV422;
    const bool tall_mcu = info.sample_method == JPEG_DOWN_SAMPLING_YUV420;
    const uint32_t bw = wide_mcu ? 16 : 8, bh = tall_mcu ? 16 : 8;
    const uint32_t stride = (info.width + bw - 1) / bw * bw;
    const uint32_t rows = (info.height + bh - 1) / bh * bh;

    jpeg_decode_memory_alloc_cfg_t in_cfg{};
    in_cfg.buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER;
    size_t in_cap = 0;
    auto *in = static_cast<uint8_t *>(jpeg_alloc_decoder_mem(len, &in_cfg, &in_cap));
    jpeg_decode_memory_alloc_cfg_t out_cfg{};
    out_cfg.buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER;
    size_t out_cap = 0;
    auto *raw = static_cast<uint8_t *>(jpeg_alloc_decoder_mem(stride * rows * 2, &out_cfg, &out_cap));
    auto *dst = static_cast<uint16_t *>(
        heap_caps_malloc(static_cast<size_t>(px) * px * 2, MALLOC_CAP_SPIRAM));
    uint16_t *result = nullptr;
    if (in && raw && dst) {
      std::memcpy(in, data, len);
      jpeg_decode_cfg_t dcfg{};
      dcfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
      dcfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;  // little-endian, as LVGL draws
      dcfg.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
      uint32_t out_len = 0;
      if (jpeg_decoder_process(jpeg_, &dcfg, in, len, raw, out_cap, &out_len) == ESP_OK) {
        art_fit_square(reinterpret_cast<const uint16_t *>(raw), info.width, info.height, stride,
                       dst, px);
        result = dst;
        dst = nullptr;
      } else {
        ESP_LOGW(kTag, "decode failed (progressive JPEG?): %s", url.c_str());
      }
    } else {
      ESP_LOGW(kTag, "out of memory decoding %ux%u", static_cast<unsigned>(info.width),
               static_cast<unsigned>(info.height));
    }
    heap_caps_free(in);
    heap_caps_free(raw);
    heap_caps_free(dst);
    return result;
  }
};

inline MediaArt &media_art() {
  static MediaArt art;
#ifdef TILEHAUS_HA_URL
  static const bool based = (art.set_ha_base(TILEHAUS_HA_URL), true);  // tilehaus_art's ha_url
  (void) based;
#endif
  return art;
}

}  // namespace tilehaus
