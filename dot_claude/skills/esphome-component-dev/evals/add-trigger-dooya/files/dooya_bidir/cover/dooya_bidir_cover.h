#pragma once

#include "esphome/components/cover/cover.h"
#include "esphome/core/component.h"
#include "../dooya_bidir.h"

namespace esphome::dooya_bidir {

class DooyaBidirCover : public cover::Cover, public Component, public DooyaDevice {
 public:
  void setup() override;
  void dump_config() override;

  cover::CoverTraits get_traits() override;
  void on_state_changed(const MotorState &state, uint8_t fields) override;

 protected:
  void control(const cover::CoverCall &call) override;
};

}  // namespace esphome::dooya_bidir
