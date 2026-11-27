import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv

from .. import DEVICE_SCHEMA, DooyaDevice, dooya_bidir_ns, register_device

DEPENDENCIES = ["dooya_bidir"]

DooyaBidirCover = dooya_bidir_ns.class_(
    "DooyaBidirCover", cover.Cover, cg.Component, DooyaDevice
)

CONFIG_SCHEMA = (
    cover.cover_schema(DooyaBidirCover, device_class="shade")
    .extend(DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await register_device(var, config)
