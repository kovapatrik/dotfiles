---
name: esphome-component-dev
description: Write, extend and review ESPHome components (core or external_components) the way the official ESPHome developer docs (developers.esphome.io) prescribe - Python config schema and codegen (__init__.py, platform files), C++ component structure, entity platforms (sensor, binary_sensor, switch, select, button, number, cover, text_sensor...), logging, automations (triggers/actions), non-blocking design, memory rules and test configs. Use this whenever the user works on ESPHome component code: creating a new component or platform, adding an entity, trigger or action to an existing component, porting an Arduino library to ESPHome, fixing codegen/validation errors, or asking "is this the ESPHome way?" / for a review against ESPHome conventions - even if they don't mention the developer docs.
---

# ESPHome component development

The ESPHome developer docs at https://developers.esphome.io are the source of truth for how components should be built. This skill condenses them into references and a workflow. The docs track ESPHome's `dev` branch and change often, so treat the references as a map and check the live page when a detail matters.

## 1. Know the target ESPHome version first

Many helpers in the docs are tagged "2026.10.0+", "2026.11.0+" and so on. Code that uses a helper the user's ESPHome doesn't have fails to compile, so before using any version-tagged API:

1. Find the version: `esphome version` (activate the project's venv first if there is one), or the pinned version in `pyproject.toml` / `requirements.txt`.
2. Find the installed source: `python -c "import esphome, os; print(os.path.dirname(esphome.__file__))"`. Grep it to confirm a helper exists (e.g. `grep -rn "def add_optional" <path>/cpp_generator.py`).
3. If it's missing, use the older pattern the reference files mention next to it. For an external component that must support several releases, prefer the older pattern or add an `#ifndef` fallback.

The installed source is also the best set of examples: `<path>/components/` holds every in-tree component. Before writing something new, find one or two components with the same shape (same bus, same entity mix, hub + platforms) and follow them.

## 2. Pick the shape

| Situation | Shape |
|---|---|
| One device, one or two values | Single component, entity sub-configs (`temperature:`, `humidity:`) |
| One connection, many independent entities | Hub component + platform files per entity domain (`sensor/`, `switch/`...), children reference the hub by id |
| Entity only calls a hub method | `Apply*` classes (`ApplyButton`, `ApplySwitch`, `ApplySelect`, `ApplyNumber`...) - no C++ class needed (check the version) |
| Needs a bus | Use the bus helpers (`i2c.new_i2c_component`, `uart.new_uart_component`, `spi.new_spi_component`), never raw drivers |

Details and trade-offs: `references/entities.md`.

## 3. Write it

Read the reference file for the part you're touching:

- `references/python-codegen.md` - `__init__.py` metadata (`DOMAIN`, `CODEOWNERS`, `DEPENDENCIES`, `AUTO_LOAD`, `MULTI_CONF`...), `CONFIG_SCHEMA`, `to_code`, validators, final validation.
- `references/cpp-structure.md` - header/source layout, lifecycle (`setup`/`loop`/`update`/`dump_config`), status reporting, scheduler and loop control, preferences, code style, memory rules.
- `references/entities.md` - entity schemas, publish contracts, multiple entities, per-domain notes (sensor, switch, select, button, cover...).
- `references/logging.md` - tags, levels, `dump_config` formatting, hex dumps.
- `references/automations.md` - triggers, actions, conditions, templatable values.
- `references/testing.md` - test YAML layout, how to compile-check.
- `references/doc-index.md` - every relevant docs URL, for fetching the live page.

The rules that come up most often, with the reason behind each:

- **Never block** in `setup()`, `loop()` or `update()`; no delays over 10 ms. Everything shares one loop, so a blocking component stalls Wi-Fi, the API and every other component. Use a state machine, `set_timeout` or `defer`.
- **No heap allocation after `setup()`** (no growing `std::vector`/`std::string`, no `std::map` for small sets, no `std::deque`). Long-running devices fragment their heap and crash days later. Prefer `std::array`, `StaticVector`, `FixedVector`, stack buffers and the `*_to()` / `*_into_buffer()` helpers.
- **`this->` on every member access, members end in `_`**, `lower_snake_case` functions, `UpperCamelCase` types, `UPPER_SNAKE_CASE` constants, `using` instead of `typedef`, no `#define` constants. CI and reviewers enforce this.
- **`dump_config()` in every component**, printing only configuration and values found in `setup()`, with no bus I/O.
- **Namespace = folder name = `DOMAIN`**: `esphome::<name>` in C++, `cg.esphome_ns.namespace("<name>")` in Python.
- **Entities are optional** in the schema, and hardware values are published raw: users add filters.
- **Mark leaf classes `final`** (component/platform classes, `Action`/`Trigger`/`Condition` subclasses), but never hubs or base classes other code derives from.
- **Use the helpers**, not hand-written equivalents: schema builders (`sensor.sensor_schema(...)`), `new_*`/`register_*` codegen, `helpers.h` (`crc16`, `put_be16`, `format_hex_pretty_to`...), `status_*` reporting.

## 4. Verify

1. Validate the config: `esphome config <file>.yaml`.
2. Compile it: `esphome compile <file>.yaml`. For in-tree work: `script/test_build_components -e compile -c <component>`.
3. Make sure a test YAML exercises every option and every automation (layout in `references/testing.md`). Compiling is the only check that catches codegen mistakes.
4. Read your own `dump_config()` output and log lines against `references/logging.md`.

## Reviewing existing code

When asked to review a component against the guidelines, go through the reference files as a checklist and report findings as `file:line - rule - why it matters - suggested fix`, most important first: blocking calls and runtime heap use, then incorrect publish/state contracts, then structure and codegen issues, then style. Separate "violates the docs" from "matches the docs but could be simpler". Don't flag things the user's ESPHome version can't do yet; mention them as "available from <version>" instead.
