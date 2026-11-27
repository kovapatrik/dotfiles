# Testing and verification

Sources: https://developers.esphome.io/architecture/ci/component_tests/ , /contributing/code/ (testing section)

## In-tree components (esphome/esphome)

```
tests/components/<name>/
├── common.yaml               # shared config: the component, every option, every automation
├── test.esp32-idf.yaml       # per platform: substitutions (pins) + packages: !include common.yaml
├── test.esp32-ard.yaml
├── test.esp32-c3-idf.yaml
├── test.esp8266-ard.yaml
├── test.rp2040-ard.yaml
└── validate.<platform>.yaml  # optional: config validation only, never compiled
```

- One test per framework/architecture combination the component supports.
- Tests only prove it compiles, so they must exercise every option and every trigger/action, or codegen bugs slip through.
- Run: `script/test_build_components -e compile -c <name>` (`-e config` = validation only).
- Lint: `prek run --all-files`; C++ static analysis: `script/clang-tidy --changed --all-headers`; Python: `pytest tests/unit_tests`.

## External components

- Keep an example/test YAML that loads the component via `external_components: - source: {type: local, path: components}` and enables every option, entity and automation.
- `esphome config test.yaml` for validation, `esphome compile test.yaml` for codegen + C++.
- Pure logic (parsers, protocol code) is easiest to test as a plain C++ unit test that compiles without ESPHome, if the code is kept free of ESPHome includes.
- clang-format with ESPHome's `.clang-format` (copy it from the esphome repo) keeps formatting aligned.
