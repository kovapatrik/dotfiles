# C++ side: structure, lifecycle, style, memory

Sources: https://developers.esphome.io/architecture/components/ , /contributing/code/ , /architecture/components/advanced/

## Header and source

```cpp
// my_comp.h
#pragma once
#include "esphome/core/component.h"

namespace esphome::my_comp {

class MyComp final : public Component {   // final only for leaf classes (not hubs/base classes)
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_address(uint32_t address) { this->address_ = address; }   // setters run before setup()

 protected:
  uint32_t address_{0};
};

}  // namespace esphome::my_comp
```

```cpp
// my_comp.cpp
#include "my_comp.h"
#include "esphome/core/log.h"

namespace esphome::my_comp {

ESPHOME_LOG_TAG(TAG, "my_comp");   // 2026.10.0+; older: static const char *const TAG = "my_comp";

void MyComp::setup() { ... }
void MyComp::dump_config() { ESP_LOGCONFIG(TAG, "My Comp:\n  Address: 0x%08" PRIX32, this->address_); }

}  // namespace esphome::my_comp
```

## Lifecycle

| Method | Rule |
|---|---|
| `setup()` | One-time init (incl. probing the bus). Runs once, ordered by `get_setup_priority()` (higher runs earlier; default `setup_priority::DATA`; bus devices after the bus) |
| `loop()` | Called every loop pass (~16 ms in current releases). Must not block. Call `this->disable_loop()` when idle and `this->enable_loop()` when work arrives (`enable_loop_soon_any_context()` from ISRs/other threads) |
| `update()` | `PollingComponent` only, at the configured `update_interval` |
| `dump_config()` | Required. Logs config and values found in `setup()`. No bus reads/writes |
| `on_shutdown()` / `on_safe_shutdown()` / `teardown()` | Clean-up before reboot/OTA/deep sleep |

**Never block**: no `delay()` over 10 ms, no busy-waits on hardware. A pass over ~50 ms logs a "took a long time" warning. Split work into a state machine, or use the scheduler:

| Need | Use |
|---|---|
| One-shot later, or self-rescheduling with variable delay | `this->set_timeout(ms, [this]() {...})` |
| Fixed cadence ≥ 500 ms | `this->set_interval(ms, ...)` |
| Run on the next loop pass (leave ISR context, break recursion) | `this->defer(...)` |
| Cadence < 250 ms | `loop()` gated by `App.get_loop_component_start_time()` (not `millis()`) |
| Cadence < 16 ms | `HighFrequencyLoopRequester` |

Named timers (to cancel/replace): `static constexpr uint32_t` ids, scoped per component (`this->cancel_timeout(ID)`). Background threads/callbacks: queue the work and `App.wake_loop_threadsafe()`.

## Status reporting

- `this->status_set_warning(LOG_STR("reason"))` / `status_clear_warning()`; same for `error`. A component that sets a warning must clear it on success.
- `status_momentary_warning(...)` clears itself (default 5 s).
- `this->status_comm_result(ok)` sets/clears the warning from a bus result and returns `ok`. (`status_comm_check` 2026.12+.)
- `this->mark_failed()` (or `setup_check(ok)`) when setup can't succeed; the component then stops running.

## Persisting state

`this->pref_ = global_preferences->make_preference<MyStruct>(hash, in_flash)`; `load()` / `save()` return false on failure (treat as first boot). `in_flash=false` = RTC (survives reboot, not power loss); `true` = flash (wears with writes, so save on change, not periodically). Entities: `make_entity_preference<T>(version, in_flash)`.

## Code style (enforced by CI and review)

- Google C++ style with ESPHome changes; 2-space indent, ≤120 columns, clang-format.
- `lower_snake_case` functions/variables, `UpperCamelCase` types, `UPPER_SNAKE_CASE` constants, members `lower_snake_case_` with a trailing underscore.
- `this->` on **every** member access (fields and methods).
- Fields `protected` by default; `private` only when a setter must keep invariants.
- `using type_t = ...;`, not `typedef`. `#define` only for `#ifdef` gates and generated sizes; constants are `static constexpr`.
- No `static` variables in component classes (instances must be independent). Don't reach into other components directly (e.g. publishing to MQTT).
- `final` on leaf classes only; adding `final` to an existing public class is a breaking change.
- No commented-out code blocks; link the datasheet for new hardware.
- Use the abstractions (I2C/SPI/UART devices, entity base classes) and wrap any library so it goes through them; avoid libraries with global state.

## Memory and flash

- **No heap allocation after `setup()`.** Fragmentation crashes long-running devices.
- Fixed size → `std::array` (size via generated define); fixed capacity → `StaticVector`; size known at setup → `FixedVector`; byte buffers → `std::unique_ptr<uint8_t[]>` instead of `std::vector<uint8_t>`.
- 1-16 entries: vector of small structs + linear search instead of `std::map`/`std::set`/`std::unordered_map`. Never `std::deque` (≥512 B). Queues: `StaticRingBuffer` / `FixedRingBuffer`.
- Avoid `std::string`-returning helpers at runtime; use `format_hex_to()`, `format_hex_pretty_to()`, `format_mac_addr_upper()`, `get_mac_address_into_buffer()` with stack buffers.
- `helpers.h` has `crc16`, `sum_bytes`, `xor_bytes`, `put_be16`/`put_le32`, `sign_extend<BITS>()`, `assign_bits()`; use them instead of hand-rolled bit code.
- ESP8266: constant tables `PROGMEM` (read via `progmem_read_byte()` / `progmem_memcpy()`); fixed string sets `PROGMEM_STRING_TABLE`.
- Gate niche features behind `USE_*` defines (Python `cg.add_define` + C++ `#ifdef`) so other users don't pay for them.
