#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "../dooya_bidir.h"

namespace esphome::dooya_bidir {

// Signal repeater (REG_RADIO, REPEATER_BIT), set with ACT_REPEATER_ON / ACT_REPEATER_OFF
class DooyaRepeaterSwitch : public switch_::Switch, public Component, public DooyaDevice {
 public:
  void dump_config() override;
  void on_refresh() override;
  void on_state_changed(const MotorState &state, uint8_t fields) override;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::dooya_bidir
