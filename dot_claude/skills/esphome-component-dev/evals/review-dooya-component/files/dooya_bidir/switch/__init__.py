import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import DEVICE_SCHEMA, DooyaDevice, dooya_bidir_ns, register_device

DEPENDENCIES = ["dooya_bidir"]

CONF_REPEATER = "repeater"

DooyaRepeaterSwitch = dooya_bidir_ns.class_(
    "DooyaRepeaterSwitch", switch.Switch, cg.Component, DooyaDevice
)

CONFIG_SCHEMA = DEVICE_SCHEMA.extend(
    {
        # Signal repeater: the motor relays radio frames for others (remote: P2, then hold P2 5 s)
        cv.Optional(CONF_REPEATER): switch.switch_schema(
            DooyaRepeaterSwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:access-point",
            default_restore_mode="DISABLED",  # the motor stores the setting
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    if conf := config.get(CONF_REPEATER):
        var = await switch.new_switch(conf)
        await cg.register_component(var, conf)
        await register_device(var, config)
