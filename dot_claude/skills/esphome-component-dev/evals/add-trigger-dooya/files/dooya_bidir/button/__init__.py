import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ACTION

from .. import (
    BRIDGE_SCHEMA,
    CONF_DOOYA_BIDIR_ID,
    CONF_MOTOR_ID,
    DooyaBidir,
    DooyaMotor,
    dooya_bidir_ns,
)

DEPENDENCIES = ["dooya_bidir"]

DooyaBidirButton = dooya_bidir_ns.class_(
    "DooyaBidirButton", button.Button, cg.Parented.template(DooyaBidir)
)
DooyaAction = dooya_bidir_ns.enum("DooyaAction")

ACTION_PAIR = "pair"
ACTION_DUMP_REGISTERS = "dump_registers"  # not a motor action: logs a register dump
CONF_FIRST = "first"
CONF_LAST = "last"
# Motor actions (confirmed on a DM45EV/S)
ACTIONS = {
    ACTION_PAIR: DooyaAction.ACT_PAIR,  # bridge button: pairs a motor in P2 mode
    **{f"favourite_{n}": getattr(DooyaAction, f"ACT_FAVOURITE_{n}") for n in range(1, 5)},
    **{f"save_favourite_{n}": getattr(DooyaAction, f"ACT_SAVE_FAVOURITE_{n}") for n in range(1, 5)},
    **{f"delete_favourite_{n}": getattr(DooyaAction, f"ACT_DELETE_FAVOURITE_{n}") for n in range(1, 5)},
    "step_up": DooyaAction.ACT_STEP_UP,
    "step_down": DooyaAction.ACT_STEP_DOWN,
}


def validate_button(config):
    if config[CONF_ACTION] != ACTION_DUMP_REGISTERS and (
        CONF_FIRST in config or CONF_LAST in config
    ):
        raise cv.Invalid("first/last only apply to action: dump_registers")
    if config.get(CONF_FIRST, 0x00) > config.get(CONF_LAST, 0xFF):
        raise cv.Invalid("first must not be after last")
    if config[CONF_ACTION] == ACTION_PAIR:
        if CONF_MOTOR_ID in config:
            raise cv.Invalid("The pair button belongs to the bridge, remove motor_id")
    elif CONF_MOTOR_ID not in config:
        raise cv.Invalid(f"action: {config[CONF_ACTION]} needs a motor_id")
    return config


CONFIG_SCHEMA = cv.All(
    button.button_schema(DooyaBidirButton)
    .extend(BRIDGE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_ACTION, default=ACTION_PAIR): cv.one_of(
                *ACTIONS, ACTION_DUMP_REGISTERS, lower=True
            ),
            cv.Optional(CONF_MOTOR_ID): cv.use_id(DooyaMotor),
            # dump_registers: register range, default the whole map
            cv.Optional(CONF_FIRST): cv.hex_uint8_t,
            cv.Optional(CONF_LAST): cv.hex_uint8_t,
        }
    ),
    validate_button,
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_parented(var, config[CONF_DOOYA_BIDIR_ID])
    if config[CONF_ACTION] == ACTION_DUMP_REGISTERS:
        cg.add(var.set_dump_range(config.get(CONF_FIRST, 0x00), config.get(CONF_LAST, 0xFF)))
    else:
        cg.add(var.set_action(ACTIONS[config[CONF_ACTION]]))
    if CONF_MOTOR_ID in config:
        motor = await cg.get_variable(config[CONF_MOTOR_ID])
        cg.add(var.set_motor(motor))
