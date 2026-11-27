#include "dooya_bidir_button.h"

#include "esphome/core/log.h"

namespace esphome::dooya_bidir {

static const char *const TAG = "dooya_bidir.button";

void DooyaBidirButton::press_action() {
  if (this->dump_) {
    if (this->motor_ != nullptr)
      this->motor_->dump_registers(this->dump_first_, this->dump_last_);  // logs its own errors
    return;
  }
  bool queued;
  if (this->action_ == ACT_PAIR) {
    queued = this->parent_->pair();
  } else if (this->motor_ != nullptr) {
    queued = this->motor_->send_action(this->action_);
  } else {
    queued = false;
  }
  if (!queued)
    ESP_LOGW(TAG, "Couldn't queue action %02X", this->action_);
}

}  // namespace esphome::dooya_bidir
