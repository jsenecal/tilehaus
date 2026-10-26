"""Cover art for the Media card.

The card downloads each player's cover itself (firmware/native/media_art.h),
with ESP-IDF's HTTP client. ESPHome leaves that client out of the build unless
http_request is used, so this component brings it back.

ha_url is Home Assistant's address. HA serves some covers itself, as a path
on its own server (its media player proxy), and the panel needs the address to
fetch those. Music Assistant and most streaming services give a full URL and
need nothing.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32 import include_builtin_idf_component
from esphome.cpp_generator import RawExpression
from esphome.helpers import cpp_string_escape

CODEOWNERS = ["@jsenecal"]

CONF_HA_URL = "ha_url"


def _base_url(value):
    value = cv.string_strict(value).strip()
    if value and not value.startswith(("http://", "https://")):
        raise cv.Invalid("ha_url must start with http:// or https://")
    return value.rstrip("/")


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_HA_URL, default=""): _base_url,
    }
)


async def to_code(config):
    include_builtin_idf_component("esp_http_client")
    cg.add_define("TILEHAUS_HA_URL", RawExpression(cpp_string_escape(config[CONF_HA_URL])))
