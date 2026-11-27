import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import DEVICE_SCHEMA, DooyaDevice, dooya_bidir_ns, register_device

DEPENDENCIES = ["dooya_bidir"]

CONF_DIRECTION = "direction"

DooyaDirectionSelect = dooya_bidir_ns.class_(
    "DooyaDirectionSelect", select.Select, cg.Component, DooyaDevice
)

# Same order as in the C++ class
DIRECTION_OPTIONS = ["Normal", "Reversed"]

CONFIG_SCHEMA = DEVICE_SCHEMA.extend(
    {
        cv.Optional(CONF_DIRECTION): select.select_schema(
            DooyaDirectionSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:swap-vertical",
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    if conf := config.get(CONF_DIRECTION):
        var = await select.new_select(conf, options=DIRECTION_OPTIONS)
        await cg.register_component(var, conf)
        await register_device(var, config)
