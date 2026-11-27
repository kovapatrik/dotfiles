import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_MODEL, ENTITY_CATEGORY_DIAGNOSTIC

from .. import DEVICE_SCHEMA, DooyaDevice, dooya_bidir_ns, register_device

DEPENDENCIES = ["dooya_bidir"]

CONF_SENSITIVITY = "sensitivity"

DooyaModelTextSensor = dooya_bidir_ns.class_(
    "DooyaModelTextSensor", text_sensor.TextSensor, cg.Component, DooyaDevice
)
DooyaSensitivityTextSensor = dooya_bidir_ns.class_(
    "DooyaSensitivityTextSensor", text_sensor.TextSensor, cg.Component, DooyaDevice
)

CONFIG_SCHEMA = DEVICE_SCHEMA.extend(
    {
        # Model name stored in the motor, e.g. "DM45EV/S"
        cv.Optional(CONF_MODEL): text_sensor.text_sensor_schema(
            DooyaModelTextSensor,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:information-outline",
        ).extend(cv.COMPONENT_SCHEMA),
        # Obstacle sensitivity: High / Medium / Low (set with the remote: P2, Stop, Down)
        cv.Optional(CONF_SENSITIVITY): text_sensor.text_sensor_schema(
            DooyaSensitivityTextSensor,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:hand-back-right-off-outline",
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    for key in (CONF_MODEL, CONF_SENSITIVITY):
        if conf := config.get(key):
            var = await text_sensor.new_text_sensor(conf)
            await cg.register_component(var, conf)
            await register_device(var, config)
