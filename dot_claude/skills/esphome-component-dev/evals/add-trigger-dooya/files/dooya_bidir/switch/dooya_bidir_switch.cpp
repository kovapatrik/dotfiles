#include "dooya_bidir_switch.h"

#include "esphome/core/log.h"

namespace esphome::dooya_bidir {

static const char *const TAG = "dooya_bidir.switch";

void DooyaRepeaterSwitch::dump_config() {
  LOG_SWITCH("", "Dooya signal repeater", this);
  ESP_LOGCONFIG(TAG, "  Motor: '%s' (%08" PRIX32 ")", this->motor_->get_name(), this->motor_->get_address());
}

void DooyaRepeaterSwitch::on_refresh() { this->motor_->read(REG_RADIO, 1); }

// Not published here: the state follows when the register is read back
void DooyaRepeaterSwitch::write_state(bool state) { this->motor_->set_repeater(state); }

void DooyaRepeaterSwitch::on_state_changed(const MotorState &state, uint8_t fields) {
  if ((fields & FIELD_REPEATER) && state.repeater.has_value())
    this->publish_state(*state.repeater);
}

}  // namespace esphome::dooya_bidir
