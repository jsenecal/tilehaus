"""Tell Home Assistant where the panel's web UI is.

Tilehaus serves its configurator from its own HTTP server, not ESPHome's
web_server component, so ESPHome never reports a web port. This component
reports one: the native API then sends it to Home Assistant, which shows a
"Visit" link on the device page, and mDNS announces an _http service.

mDNS sizes its service list from the components in the config, counting
web_server. This one is counted the same way, so the extra _http entry has
room and doesn't overwrite the API's.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import mdns
from esphome.const import CONF_PORT

CODEOWNERS = ["@jsenecal"]
DEPENDENCIES = ["api"]

DOMAIN = "tilehaus_web"

if not isinstance(getattr(mdns, "COMPONENTS_WITH_MDNS_SERVICES", None), tuple):
    raise RuntimeError(
        "tilehaus_web: esphome.components.mdns no longer has "
        "COMPONENTS_WITH_MDNS_SERVICES; the mDNS service count needs a new hook"
    )
if DOMAIN not in mdns.COMPONENTS_WITH_MDNS_SERVICES:
    mdns.COMPONENTS_WITH_MDNS_SERVICES = (*mdns.COMPONENTS_WITH_MDNS_SERVICES, DOMAIN)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PORT, default=80): cv.port,
    }
)


def _no_web_server(config):
    if "web_server" in fv.full_config.get():
        raise cv.Invalid("tilehaus_web reports the Tilehaus server's port; remove web_server")
    return config


FINAL_VALIDATE_SCHEMA = _no_web_server


async def to_code(config):
    cg.add_define("USE_WEBSERVER")
    cg.add_define("USE_WEBSERVER_PORT", config[CONF_PORT])
