#pragma once

#include "esphome/components/button/button.h"
#include "../dooya_bidir.h"

namespace esphome::dooya_bidir {

// ACT_PAIR: bridge button, pairs a motor in P2 mode. Any other action: sent to `motor_`.
// With a dump range set: logs a register dump of `motor_` instead.
class DooyaBidirButton : public button::Button, public Parented<DooyaBidir> {
 public:
  void set_action(DooyaAction action) { this->action_ = action; }
  void set_motor(DooyaMotor *motor) { this->motor_ = motor; }
  void set_dump_range(uint8_t first, uint8_t last) {
    this->dump_ = true;
    this->dump_first_ = first;
    this->dump_last_ = last;
  }

 protected:
  void press_action() override;

  DooyaAction action_{ACT_PAIR};
  DooyaMotor *motor_{nullptr};
  bool dump_{false};
  uint8_t dump_first_{0x00};
  uint8_t dump_last_{0xFF};
};

}  // namespace esphome::dooya_bidir
