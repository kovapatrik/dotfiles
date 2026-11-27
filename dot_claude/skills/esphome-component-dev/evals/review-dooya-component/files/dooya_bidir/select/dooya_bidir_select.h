#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "../dooya_bidir.h"

namespace esphome::dooya_bidir {

// Motor direction (REG_DIRECTION). Options: 0 = Normal, 1 = Reversed.
class DooyaDirectionSelect : public select::Select, public Component, public DooyaDevice {
 public:
  void dump_config() override;
  void on_refresh() override;
  void on_state_changed(const MotorState &state, uint8_t fields) override;

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::dooya_bidir
