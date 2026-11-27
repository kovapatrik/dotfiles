# Python side: `__init__.py`, schema, codegen

Source: https://developers.esphome.io/architecture/components/ and https://developers.esphome.io/contributing/code/ (Python style).

## Layout

```
components/<name>/
├── __init__.py        # top-level component (or hub): metadata, CONFIG_SCHEMA, to_code
├── <name>.h / .cpp    # C++
└── sensor.py          # or sensor/__init__.py (+ .h/.cpp): one per entity domain the component provides
```

The YAML key equals the folder name. Platform files are used as `sensor: - platform: <name>`.

## Module metadata (top-level `__init__.py`)

| Constant | Meaning |
|---|---|
| `DOMAIN = "<name>"` | Required in current ESPHome (CI lint), equals the folder name. Import it elsewhere instead of repeating the string. Keep metadata alphabetical |
| `CODEOWNERS = ["@user"]` | GitHub users |
| `DEPENDENCIES = ["uart"]` | Must be configured, or validation fails |
| `AUTO_LOAD = ["sensor"]` | Loaded automatically; may be a callable inspecting `CORE`. **Not allowed for the main entity platforms** (`sensor`, `binary_sensor`, `switch`...) in-tree |
| `CONFLICTS_WITH` | Error if configured together |
| `MULTI_CONF = True` (or a number) | Allows several instances (hubs usually need this) |

Rarer: `MULTI_CONF_NO_DEFAULT`, `ALIASES`/`ALIAS_REMOVAL_VERSION`, `FINAL_VALIDATE_SCHEMA`, `IS_PLATFORM_COMPONENT`, `PREFETCH_FILES`.

## Namespace and classes

```python
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

my_ns = cg.esphome_ns.namespace("my_comp")           # must match the folder / C++ namespace
MyComp = my_ns.class_("MyComp", cg.Component)          # list every C++ base class
MyHub = my_ns.class_("MyHub", cg.PollingComponent, uart.UARTDevice)
```

## CONFIG_SCHEMA

```python
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MyComp),
        cv.Required(CONF_ADDRESS): cv.hex_uint32_t,
        cv.Optional(CONF_TIMEOUT, default="1s"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)          # or cv.polling_component_schema("60s"), uart.UART_DEVICE_SCHEMA, ...
```

- Common validators: `cv.Required`, `cv.Optional`, `cv.boolean`, `cv.string`, `cv.int_range`, `cv.one_of`, `cv.positive_time_period_milliseconds`, `cv.use_id(OtherClass)`, `cv.ensure_list`, `cv.All(...)`.
- Enums: `cv.enum(MyEnum.options(prefix=..., upper=True))` maps YAML strings to C++ enumerators (`<Enum>.options` is newer; otherwise a dict of `name: ns.MY_ENUM_VALUE`).
- Dependent keys: `cv.only_when(...)` / `cv.key_requires(...)` inside `cv.All()`. Don't give a restricted key a `default`. When most of the schema depends on one key, use `cv.typed_schema({...}, key=CONF_TYPE)`.
- `cv.Optional(..., visibility=...)` (`ADVANCED`, `YAML_ONLY`) only affects editors; `cv.sensitive(...)` for secrets.
- Config keys: `CONF_XYZ` constants. Import from `esphome.const` when it exists there; otherwise define locally. (In-tree: shared by 2+ components → `esphome/components/const/__init__.py`.)
- Cross-component checks: `FINAL_VALIDATE_SCHEMA` with `esphome.final_validate` helpers (`fv.get_declaring_config`, `fv.platform_entries`...).

## to_code

```python
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_address(config[CONF_ADDRESS]))
    if (timeout := config.get(CONF_TIMEOUT)) is not None:   # walrus for optional keys (not for booleans)
        cg.add(var.set_timeout(timeout))
```

- Bus components: `await i2c.new_i2c_component(config)` / `uart.new_uart_component` / `spi.new_spi_component` create **and** register.
- References: `parent = await cg.get_variable(config[CONF_PARENT_ID])`; for `Parented<T>` children `await cg.register_parented(var, config[CONF_PARENT_ID])`.
- Pins: `pin = await cg.gpio_pin_expression(config[CONF_PIN]); cg.add(var.set_pin(pin))`.
- Version-tagged shortcuts (2026.11.0+): `cg.add_optional(setter, value)` skips `None`; `cg.add_pin(...)` / `cg.add_variable(...)` accept `None`. On older versions use `if ... is not None`.
- Feature gating: `cg.add_define("USE_MY_FEATURE")` only in the branch where the user enabled it, plus `#ifdef USE_MY_FEATURE` in C++. Use `cg.add_build_flag()` only for flags that don't change header layout; `cg.add_library()` for PlatformIO libraries (avoid libraries where possible).
- Compile-time sizes for `std::array`: generate a define with `cg.add_define("MY_COUNT", n)`.

## Platform files (e.g. `sensor.py`)

```python
from esphome.components import sensor
from . import MyHub, my_ns, CONF_MY_HUB_ID

DEPENDENCIES = ["my_comp"]

CONFIG_SCHEMA = sensor.sensor_schema(MySensor, unit_of_measurement=..., accuracy_decimals=1).extend(
    {cv.GenerateID(CONF_MY_HUB_ID): cv.use_id(MyHub)}
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_MY_HUB_ID])
```

See `entities.md` for per-domain schema builders and the multi-entity patterns.
