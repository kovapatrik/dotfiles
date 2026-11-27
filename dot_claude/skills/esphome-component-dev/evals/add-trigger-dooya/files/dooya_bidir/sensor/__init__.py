import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

from .. import DEVICE_SCHEMA, DooyaDevice, dooya_bidir_ns, register_device

DEPENDENCIES = ["dooya_bidir"]

CONF_RSSI = "rssi"

DooyaRssiSensor = dooya_bidir_ns.class_(
    "DooyaRssiSensor", sensor.Sensor, cg.Component, DooyaDevice
)

CONFIG_SCHEMA = DEVICE_SCHEMA.extend(
    {
        # Signal strength of every packet received from the motor
        cv.Optional(CONF_RSSI): sensor.sensor_schema(
            DooyaRssiSensor,
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    if conf := config.get(CONF_RSSI):
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)
        await register_device(var, config)
