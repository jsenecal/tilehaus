#pragma once
#include "lvgl.h"
#include <memory>
#include <string>
#include "card.h"        // CardFonts
#include "ha.h"
#include "light_modal.h"
#include "cover_modal.h"

namespace tilehaus {

// A detail ("more-info") modal for an entity, dispatched by domain. Owns the
// modal plus its own on/off subject (for the light power tab), built hidden at
// boot so its HA subscriptions register before HA's initial-state push. Extend
// by adding domains + members. make_detail_modal returns nullptr when the
// entity's domain has no detail modal.
struct DetailModal {
  lv_subject_t power_{};
  std::unique_ptr<LightModal> light_;
  std::unique_ptr<CoverModal> cover_;
  // future: std::unique_ptr<ClimateModal> climate_; etc.

  void open() {
    if (light_) light_->show();
    if (cover_) cover_->show();
  }
};

inline std::string entity_domain(const std::string &entity) {
  auto dot = entity.find('.');
  return dot == std::string::npos ? std::string() : entity.substr(0, dot);
}

inline bool has_detail_modal(const std::string &entity) {
  std::string d = entity_domain(entity);
  return d == "light" || d == "cover";
}

inline std::unique_ptr<DetailModal> make_detail_modal(const std::string &entity,
                                                      const std::string &title,
                                                      const CardFonts &fonts) {
  if (entity_domain(entity) == "light") {
    auto d = std::make_unique<DetailModal>();
    lv_subject_init_int(&d->power_, 0);
    lv_subject_t *pw = &d->power_;
    ha_subscribe(entity, nullptr, [pw](const std::string &s) {
      lv_subject_set_int(pw, (s == "on") ? 1 : 0);
    });
    d->light_.reset(new LightModal(entity, title, fonts, &d->power_));
    d->light_->build();
    return d;
  }
  if (entity_domain(entity) == "cover") {
    auto d = std::make_unique<DetailModal>();
    d->cover_.reset(new CoverModal(entity, title, fonts));
    d->cover_->build();
    return d;
  }
  return nullptr;
}

}  // namespace tilehaus
