#include "dooya_bidir_binary_sensor.h"

#include "esphome/core/log.h"

namespace esphome::dooya_bidir {

static const char *const TAG = "dooya_bidir.binary_sensor";

void DooyaConnectedBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Dooya connected", this);
  ESP_LOGCONFIG(TAG, "  Motor: '%s' (%08" PRIX32 ")", this->motor_->get_name(), this->motor_->get_address());
}

void DooyaRewindBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Dooya rewind", this);
  ESP_LOGCONFIG(TAG, "  Motor: '%s' (%08" PRIX32 ")", this->motor_->get_name(), this->motor_->get_address());
}

}  // namespace esphome::dooya_bidir
