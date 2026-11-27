import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

from .. import DEVICE_SCHEMA, DooyaDevice, dooya_bidir_ns, register_device

DEPENDENCIES = ["dooya_bidir"]

CONF_CONNECTED = "connected"
CONF_REWIND = "rewind"

DooyaConnectedBinarySensor = dooya_bidir_ns.class_(
    "DooyaConnectedBinarySensor", binary_sensor.BinarySensor, cg.Component, DooyaDevice
)
DooyaRewindBinarySensor = dooya_bidir_ns.class_(
    "DooyaRewindBinarySensor", binary_sensor.BinarySensor, cg.Component, DooyaDevice
)

CONFIG_SCHEMA = DEVICE_SCHEMA.extend(
    {
        # ON while the motor answers; OFF after DooyaMotor::OFFLINE_AFTER failed requests in a row
        cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
            DooyaConnectedBinarySensor,
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.COMPONENT_SCHEMA),
        # Rewind at obstacle (set with the remote: P2, Down, Down)
        cv.Optional(CONF_REWIND): binary_sensor.binary_sensor_schema(
            DooyaRewindBinarySensor,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:backup-restore",
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    for key in (CONF_CONNECTED, CONF_REWIND):
        if conf := config.get(key):
            var = await binary_sensor.new_binary_sensor(conf)
            await cg.register_component(var, conf)
            await register_device(var, config)
