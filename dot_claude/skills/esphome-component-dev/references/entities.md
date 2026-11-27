# Entity platforms

Sources: https://developers.esphome.io/architecture/components/sensor/ , /switch/ , and the other pages listed in `doc-index.md`. Fetch the page for the domain you're implementing; each has its own publish contract.

## Common pattern

- Schema: the domain's builder with the class as first argument, e.g. `sensor.sensor_schema(MySensor, unit_of_measurement=..., device_class=..., state_class=..., accuracy_decimals=..., entity_category=..., icon=...)`. The class argument declares the id, so no `cv.GenerateID()`. Extend with `cv.COMPONENT_SCHEMA` (or `cv.polling_component_schema("60s")`) when the entity is also a component.
- Builders by domain: `sensor_schema`, `binary_sensor_schema`, `text_sensor_schema`, `switch_schema`, `button_schema`, `select_schema`, `number_schema`, `cover.cover_schema` (check the version for cover), `text_schema`, `event_schema`.
- Codegen: `var = await <domain>.new_<domain>(config, ...)` creates the variable and applies the common options; then `await cg.register_component(var, config)` if it's a component. If a parent creates the entity, declare the id yourself and use `<domain>.register_<domain>(var, config)`.
- Make every entity optional in the schema. Publish raw values; users add filters.
- `dump_config()`: use the domain's log macro: `LOG_SENSOR("", "Temperature", this->temperature_sensor_)`, `LOG_BINARY_SENSOR`, `LOG_TEXT_SENSOR`, `LOG_SWITCH`, `LOG_BUTTON`, `LOG_SELECT`, `LOG_NUMBER`, `LOG_COVER`.
- Don't log state changes yourself; the entity base classes do (VERBOSE).

## Publish contracts

| Domain | Contract |
|---|---|
| sensor | `publish_state(float)` with the raw value; `NAN` = unavailable |
| binary_sensor / text_sensor | `publish_state(...)` |
| switch | Implement `write_state(bool state)` (inversion already applied), drive the hardware, then call `publish_state()` yourself with the state actually reached. Don't override `turn_on/turn_off/toggle`. If the state can't be read back, override `assumed_state()` → `true`. Restoring: `default_restore_mode=` in the schema; call `this->restore()` at the end of `setup()` unless the mode is `DISABLED` (e.g. the device stores the setting itself) |
| select | Implement `control(size_t index)` (newer) or `control(const std::string &)` (older; check the installed `select.h`) and `publish_state(...)` once applied |
| number | Implement `control(float)`; publish when applied |
| button | Implement `press_action()`; no state |
| cover | Implement `get_traits()` and `control(const cover::CoverCall &)`; set `position`/`current_operation` and call `publish_state()` |

"Publish once applied" means: for hardware with read-back, publish the read-back value (not the requested one) so the UI never shows a state the device didn't take.

## Several entities from one component

1. **Sub-configs on one entry** (`dht`: `temperature:`, `humidity:`): readings from one measurement cycle. The sub-schemas use the builder without a class; codegen via `sensor.sub_sensors(config)` (2026.10.0+, also for other domains) or one `new_sensor` per key with explicit setters. C++: `SUB_SENSOR(name)` → `name##_sensor_` + `set_##name##_sensor()` (`SUB_SWITCH`, ... likewise); null-check before publishing (`PUBLISH_STATE(entity, ...)` does it, 2026.11.0+).
2. **Hub + entries with a `type` key**: independent entities sharing a connection. `cv.typed_schema({...}, key=CONF_TYPE)`, each with its own class; `cg.register_parented(var, config[CONF_HUB_ID])`.
3. **Hub + platform files referencing the hub id**: each domain in its own `<domain>/__init__.py` (or `<domain>.py`), children declare `cv.use_id(Hub)`. The usual structure for radio/bus hubs.
4. **`Apply*` entities** (`ApplyButton`, `ApplySwitch`, `ApplySelect`, `ApplyNumber`, `ApplyFloatOutput`, `ApplyBinaryOutput`): an entity that only calls a hub method needs no C++ class; configure `apply=` / `parent=` / `publish=` (`before`, `after`, `on_success`). Check that the installed version has them.

Follow the pattern the surrounding component already uses when more than one fits.

## Hub ↔ child communication

- Children hold `Parented<Hub>` (`this->parent_`) or a pointer set in codegen.
- Hub → children: either a listener interface the children implement (registered in `to_code`), or `cg.HubFunction` / `HUB_PUBLISHER` (newer). Keep a listener class only when the child has its own state or timing.
- Children never talk to other components directly.
