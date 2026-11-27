#include "dooya_bidir_sensor.h"

#include "esphome/core/log.h"

namespace esphome::dooya_bidir {

static const char *const TAG = "dooya_bidir.sensor";

void DooyaRssiSensor::dump_config() {
  LOG_SENSOR("", "Dooya RSSI", this);
  ESP_LOGCONFIG(TAG, "  Motor: '%s' (%08" PRIX32 ")", this->motor_->get_name(), this->motor_->get_address());
}

}  // namespace esphome::dooya_bidir
