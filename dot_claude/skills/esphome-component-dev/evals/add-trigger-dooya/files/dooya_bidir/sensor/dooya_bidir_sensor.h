#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "../dooya_bidir.h"

namespace esphome::dooya_bidir {

// RSSI of every packet received from the motor
class DooyaRssiSensor : public sensor::Sensor, public Component, public DooyaDevice {
 public:
  void dump_config() override;
  void on_message(const Message &msg, float rssi) override { this->publish_state(rssi); }
};

}  // namespace esphome::dooya_bidir
