# Logging

Source: https://developers.esphome.io/architecture/logging/best_practices/

## Tags

- One tag per `.cpp`, at namespace scope: `ESPHOME_LOG_TAG(TAG, "my_comp.sensor");` (2026.10.0+; CI rejects plain `static const char *const TAG` in-tree). External components supporting older releases: check the page for the `#ifndef ESPHOME_LOG_TAG` fallback.
- `TAG` is only for log macros: not a scheduler name, not for `strcmp`, not to build strings.

## Levels

| Macro | Use for |
|---|---|
| `ESP_LOGE` | Serious failure; things will go wrong |
| `ESP_LOGW` | Possibly bad, operation continues |
| `ESP_LOGI` | User-facing events a non-developer understands |
| `ESP_LOGD` | Needed in normal use; ESPHome's default level, so use sparingly |
| `ESP_LOGV` | Troubleshooting / commissioning detail |
| `ESP_LOGVV` | Trace data such as raw packets (usually compiled out) |

## Messages

- Short. No component name (the tag shows it), no troubleshooting essays, questions or extra punctuation. Every string costs flash and network bandwidth.
- Don't log entity state changes; the base classes already do.
- Not in tight loops: log on change or periodically.
- Integers: `PRIu32`/`PRIX32`/... from `<cinttypes>` for fixed-width types (`"%" PRIu32`).
- Raw bytes: `ESP_LOGD_HEX` / `ESP_LOGV_HEX` / `ESP_LOGVV_HEX(TAG, data, len, MAX_BYTES, "fmt %s")` (constant `MAX_BYTES`; the hex text is the last `%s`), or a stack buffer with `format_hex_pretty_to()`. Never `format_hex_pretty()` returning `std::string` at runtime.

## dump_config

- `ESP_LOGCONFIG` only. Combine related lines into **one call** with `\n` and two-space indentation (one network packet instead of many); the whole formatted message must fit in 512 bytes.
- Optional fields in separate `if` + `ESP_LOGCONFIG` calls.
- Entities via `LOG_SENSOR("  ", "Temperature", this->temperature_sensor_)` etc.
- Don't build the text with `str_sprintf` / `std::string`.

```cpp
void MyComp::dump_config() {
  ESP_LOGCONFIG(TAG,
                "My Comp:\n"
                "  Address: 0x%02X\n"
                "  Mode: %s",
                this->address_, this->mode_str_());
  if (this->timeout_ != 0)
    ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 " ms", this->timeout_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}
```
