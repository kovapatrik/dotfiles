#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "../dooya_bidir.h"

namespace esphome::dooya_bidir {

// ON while the motor answers
class DooyaConnectedBinarySensor : public binary_sensor::BinarySensor, public Component, public DooyaDevice {
 public:
  void dump_config() override;
  void on_availability_changed(bool online) override { this->publish_state(online); }
};

// Rewind at obstacle (REG_REWIND)
class DooyaRewindBinarySensor : public binary_sensor::BinarySensor, public Component, public DooyaDevice {
 public:
  void dump_config() override;
  void on_refresh() override { this->motor_->read_settings(); }
  void on_state_changed(const MotorState &state, uint8_t fields) override {
    if ((fields & FIELD_REWIND) && state.rewind.has_value())
      this->publish_state(*state.rewind);
  }
};

}  // namespace esphome::dooya_bidir
