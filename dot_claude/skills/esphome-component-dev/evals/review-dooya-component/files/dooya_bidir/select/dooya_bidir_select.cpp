#include "dooya_bidir_select.h"

#include "esphome/core/log.h"

namespace esphome::dooya_bidir {

static const char *const TAG = "dooya_bidir.select";

static constexpr size_t OPTION_NORMAL = 0;
static constexpr size_t OPTION_REVERSED = 1;

void DooyaDirectionSelect::dump_config() {
  LOG_SELECT("", "Dooya direction", this);
  ESP_LOGCONFIG(TAG, "  Motor: '%s' (%08" PRIX32 ")", this->motor_->get_name(), this->motor_->get_address());
}

void DooyaDirectionSelect::on_refresh() { this->motor_->read(REG_DIRECTION, 1); }

void DooyaDirectionSelect::control(size_t index) {
  // Not published here: the state follows when the motor's direction is read back
  this->motor_->set_direction(index == OPTION_REVERSED ? DIR_REVERSED : DIR_NORMAL);
}

void DooyaDirectionSelect::on_state_changed(const MotorState &state, uint8_t fields) {
  if (!(fields & FIELD_DIRECTION) || !state.direction.has_value())
    return;
  switch (*state.direction) {
    case DIR_NORMAL:
      this->publish_state(OPTION_NORMAL);
      break;
    case DIR_REVERSED:
      this->publish_state(OPTION_REVERSED);
      break;
    default:
      ESP_LOGW(TAG, "Unknown direction value %02X", *state.direction);
      break;
  }
}

}  // namespace esphome::dooya_bidir
