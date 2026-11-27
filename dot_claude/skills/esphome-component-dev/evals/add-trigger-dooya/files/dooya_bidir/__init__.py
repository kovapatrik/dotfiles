from esphome import automation
import esphome.codegen as cg
from esphome.components import cc1101
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME

CODEOWNERS = ["@kovapatrik"]
DEPENDENCIES = ["cc1101"]

CONF_DOOYA_BIDIR_ID = "dooya_bidir_id"
CONF_CC1101_ID = "cc1101_id"
CONF_MOTORS = "motors"
CONF_MOTOR_ID = "motor_id"
CONF_ON_PAIR = "on_pair"
CONF_PAIR_CODES = "pair_codes"

# Pairing reply codes (payload 00 <code>) seen so far; they differ per motor model
DEFAULT_PAIR_CODES = [0x03, 0x05, 0x09]

dooya_bidir_ns = cg.esphome_ns.namespace("dooya_bidir")
DooyaBidir = dooya_bidir_ns.class_(
    "DooyaBidir",
    cg.PollingComponent,
    cc1101.CC1101Listener,
    cg.Parented.template(cc1101.CC1101Component),
)
DooyaMotor = dooya_bidir_ns.class_("DooyaMotor", cg.Parented.template(DooyaBidir))
DooyaDevice = dooya_bidir_ns.class_("DooyaDevice")


def validate_address(value):
    value = cv.hex_uint32_t(value)
    if value in (0x00000000, 0xFFFFFFFF):
        raise cv.Invalid("0x00000000 and 0xFFFFFFFF are reserved")
    return value


MOTOR_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(DooyaMotor),
        cv.Required(CONF_NAME): cv.string_strict,
        cv.Required(CONF_ADDRESS): validate_address,
    }
)


def validate_unique_addresses(motors):
    seen = {}
    for motor in motors:
        address = motor[CONF_ADDRESS]
        if address in seen:
            raise cv.Invalid(
                f"Motors '{seen[address]}' and '{motor[CONF_ID]}' "
                f"have the same address 0x{address:08X}"
            )
        seen[address] = motor[CONF_ID]
    return motors


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DooyaBidir),
        cv.GenerateID(CONF_CC1101_ID): cv.use_id(cc1101.CC1101Component),
        # This bridge's own ID, stored by the motors when pairing.
        # Default: derived from the ESP's MAC address (logged at boot).
        cv.Optional(CONF_ADDRESS): validate_address,
        cv.Optional(CONF_MOTORS, default=[]): cv.All(
            cv.ensure_list(MOTOR_SCHEMA), validate_unique_addresses
        ),
        cv.Optional(CONF_PAIR_CODES, default=DEFAULT_PAIR_CODES): cv.All(
            cv.ensure_list(cv.hex_uint8_t), cv.Length(min=1)
        ),
        # Fired for every accepted pairing reply.
        # Variables: address (uint32_t), pair_code (uint8_t), already_configured (bool)
        cv.Optional(CONF_ON_PAIR): automation.validate_automation({}),
    }
).extend(cv.polling_component_schema("5min"))

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_PAIR,
        "add_on_pair_callback",
        [
            (cg.uint32, "address"),
            (cg.uint8, "pair_code"),
            (cg.bool_, "already_configured"),
        ],
    ),
)

BRIDGE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DOOYA_BIDIR_ID): cv.use_id(DooyaBidir),
    }
)

# Platforms bound to one motor (cover, sensors, ...)
DEVICE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MOTOR_ID): cv.use_id(DooyaMotor),
    }
)


async def register_device(var, config):
    motor = await cg.get_variable(config[CONF_MOTOR_ID])
    cg.add(var.set_motor(motor))
    cg.add(motor.add_device(var))
    return motor


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_CC1101_ID])
    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_pair_codes(config[CONF_PAIR_CODES]))

    for motor_config in config[CONF_MOTORS]:
        motor = cg.new_Pvariable(motor_config[CONF_ID])
        cg.add(motor.set_parent(var))
        cg.add(motor.set_name(motor_config[CONF_NAME]))
        cg.add(motor.set_address(motor_config[CONF_ADDRESS]))
        cg.add(var.register_motor(motor))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
