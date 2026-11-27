#include "dooya_bidir_text_sensor.h"

#include "esphome/core/log.h"

namespace esphome::dooya_bidir {

static const char *const TAG = "dooya_bidir.text_sensor";

void DooyaModelTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Dooya model", this);
  ESP_LOGCONFIG(TAG, "  Motor: '%s' (%08" PRIX32 ")", this->motor_->get_name(), this->motor_->get_address());
}

void DooyaModelTextSensor::on_state_changed(const MotorState &state, uint8_t fields) {
  if ((fields & FIELD_MODEL) && state.model.has_value())
    this->publish_state(*state.model);
}

void DooyaSensitivityTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Dooya sensitivity", this);
  ESP_LOGCONFIG(TAG, "  Motor: '%s' (%08" PRIX32 ")", this->motor_->get_name(), this->motor_->get_address());
}

void DooyaSensitivityTextSensor::on_state_changed(const MotorState &state, uint8_t fields) {
  if (!(fields & FIELD_SENSITIVITY) || !state.sensitivity.has_value())
    return;
  switch (*state.sensitivity) {
    case SENS_MEDIUM:
      this->publish_state("Medium");
      break;
    case SENS_HIGH:
      this->publish_state("High");
      break;
    case SENS_LOW:
      this->publish_state("Low");
      break;
    default:
      ESP_LOGW(TAG, "Unknown sensitivity value %02X", *state.sensitivity);
      break;
  }
}

}  // namespace esphome::dooya_bidir
