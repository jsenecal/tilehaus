#pragma once
#include "card.h"
#include "blank_card.h"
#include "sensor_card.h"
#include "toggle_card.h"
#include "light_slider_card.h"
#include "cover_card.h"
#include "light_control_card.h"
#include "presence_card.h"
#include "weather_card.h"
#include "action_card.h"
#include "lock_card.h"
#include "header_card.h"
#include "climate_card.h"
#include "wled_card.h"
#include "option_select_card.h"
#include "fan_card.h"
#include "alarm_card.h"
#include "weather_forecast_card.h"
#include "page_card.h"
#include "back_card.h"

namespace tilehaus {

inline std::unique_ptr<Card> make_card(CardType type) {
  switch (type) {
    case CardType::Sensor: return std::unique_ptr<Card>(new SensorCard());
    case CardType::Toggle: return std::unique_ptr<Card>(new ToggleCard());
    case CardType::LightSlider: return std::unique_ptr<Card>(new LightSliderCard());
    case CardType::LightControl: return std::unique_ptr<Card>(new LightControlCard());
    case CardType::Cover: return std::unique_ptr<Card>(new CoverCard());
    case CardType::Presence: return std::unique_ptr<Card>(new PresenceCard());
    case CardType::Weather: return std::unique_ptr<Card>(new WeatherCard());
    case CardType::Scene: return std::unique_ptr<Card>(new ActionCard("scene.turn_on"));
    case CardType::Button: return std::unique_ptr<Card>(new ActionCard("button.press"));
    case CardType::Lock: return std::unique_ptr<Card>(new LockCard());
    case CardType::Header: return std::unique_ptr<Card>(new HeaderCard());
    case CardType::Climate: return std::unique_ptr<Card>(new ClimateCard());
    case CardType::Wled: return std::unique_ptr<Card>(new WledCard());
    // Door/window contact is a read-only binary-status tile — identical renderer
    // to Presence (icon swap + tint on on/off), just door glyphs + colors in the
    // deck config.
    case CardType::DoorWindow: return std::unique_ptr<Card>(new PresenceCard());
    // The fill-slider tile serves number entities too (auto-detected by domain).
    case CardType::NumberSlider: return std::unique_ptr<Card>(new LightSliderCard());
    case CardType::OptionSelect: return std::unique_ptr<Card>(new OptionSelectCard());
    case CardType::Fan: return std::unique_ptr<Card>(new FanCard());
    case CardType::Alarm: return std::unique_ptr<Card>(new AlarmCard());
    case CardType::WeatherForecast: return std::unique_ptr<Card>(new WeatherForecastCard());
    // CardType::Camera (20) is reserved but has no renderer — a stored camera
    // tile falls through to the Blank default below.
    case CardType::Page: return std::unique_ptr<Card>(new PageCard());
    case CardType::Back: return std::unique_ptr<Card>(new BackCard());
    default: return std::unique_ptr<Card>(new BlankCard());
  }
}

}  // namespace tilehaus
