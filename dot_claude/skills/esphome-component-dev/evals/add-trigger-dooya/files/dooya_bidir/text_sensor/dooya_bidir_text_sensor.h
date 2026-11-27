#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "../dooya_bidir.h"

namespace esphome::dooya_bidir {

// Model name (REG_MODEL), read once at boot
class DooyaModelTextSensor : public text_sensor::TextSensor, public Component, public DooyaDevice {
 public:
  void dump_config() override;
  void on_refresh() override { this->motor_->read_model(); }
  void on_state_changed(const MotorState &state, uint8_t fields) override;
};

// Obstacle sensitivity (REG_SENSITIVITY): High / Medium / Low
class DooyaSensitivityTextSensor : public text_sensor::TextSensor, public Component, public DooyaDevice {
 public:
  void dump_config() override;
  void on_refresh() override { this->motor_->read_settings(); }
  void on_state_changed(const MotorState &state, uint8_t fields) override;
};

}  // namespace esphome::dooya_bidir
