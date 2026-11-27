# ESPHome developer docs index

Fetch the live page when a detail matters; the docs follow the `dev` branch.

## Architecture
- Components (structure, `__init__.py`, C++ layout, status, lifecycle): https://developers.esphome.io/architecture/components/
- Advanced (loop control, scheduler, blocking warning, preferences, setup priorities): https://developers.esphome.io/architecture/components/advanced/
- Automations (triggers, actions, conditions): https://developers.esphome.io/architecture/components/automations/
- Core architecture: https://developers.esphome.io/architecture/core/
- Architectural overview (static buffers): https://developers.esphome.io/architecture/overview/
- Logging: https://developers.esphome.io/architecture/logging/overview/ and https://developers.esphome.io/architecture/logging/best_practices/
- CI and component tests: https://developers.esphome.io/architecture/ci/ , https://developers.esphome.io/architecture/ci/component_tests/

## Entity platforms
https://developers.esphome.io/architecture/components/<domain>/ for: `sensor`, `binary_sensor`, `button`, `climate`, `cover`, `switch`, `select`, `number`, `text_sensor`, `text`, `alarm_control_panel`, `display`, `event`, `fan`, `light`, `lock`, `media_player`, `microphone`, `output`, `speaker`, `time`, `touchscreen`, `valve`. Also `bluetooth`.

## Contributing
- Code style, memory rules, public API, breaking changes: https://developers.esphome.io/contributing/code/
- Development environment: https://developers.esphome.io/contributing/development-environment/
- Submitting work: https://developers.esphome.io/contributing/submitting-your-work/
- Docs (esphome.io): https://developers.esphome.io/contributing/docs/

## Elsewhere
- Starter external components: https://github.com/esphome/starter-components
- In-tree components (examples): https://github.com/esphome/esphome/tree/dev/esphome/components , or the installed package's `components/` folder
- External components (user docs): https://esphome.io/components/external_components
