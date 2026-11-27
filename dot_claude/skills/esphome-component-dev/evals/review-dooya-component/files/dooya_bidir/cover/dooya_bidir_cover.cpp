#include "dooya_bidir_cover.h"

#include "esphome/core/log.h"

namespace esphome::dooya_bidir {

static const char *const TAG = "dooya_bidir.cover";

// ESPHome: 1.0 = open, 0.0 = closed. Motor: 0 % = open, 100 % = closed.
// Reversing the direction (REG_DIRECTION) flips the motor's scale together with open/close, so the motor stays
// self-consistent and no flip is needed here.
static float motor_to_cover(float motor_percent) { return clamp(1.0f - motor_percent / 100.0f, 0.0f, 1.0f); }
static float cover_to_motor(float cover_position) { return (1.0f - cover_position) * 100.0f; }

void DooyaBidirCover::setup() {
  // The bridge reads every motor's position at boot; it arrives via on_state_changed()
}

void DooyaBidirCover::dump_config() {
  LOG_COVER("", "Dooya bidir cover", this);
  ESP_LOGCONFIG(TAG, "  Motor: '%s' (%08" PRIX32 ")", this->motor_->get_name(), this->motor_->get_address());
}

cover::CoverTraits DooyaBidirCover::get_traits() {
  cover::CoverTraits traits;
  traits.set_supports_position(true);
  traits.set_supports_stop(true);
  traits.set_is_assumed_state(false);
  return traits;
}

void DooyaBidirCover::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->motor_->send_action(ACT_STOP);
    // stays "moving" until the motor reports where it stopped
    return;
  }
  if (call.get_position().has_value()) {
    const float target = *call.get_position();
    this->motor_->send_move(cover_to_motor(target));
    if (target > this->position)
      this->current_operation = cover::COVER_OPERATION_OPENING;
    else if (target < this->position)
      this->current_operation = cover::COVER_OPERATION_CLOSING;
    this->publish_state(false);
  }
}

void DooyaBidirCover::on_state_changed(const MotorState &state, uint8_t fields) {
  if (!(fields & (FIELD_POSITION | FIELD_STATUS)))
    return;  // settings only, nothing for the cover
  if ((fields & FIELD_POSITION) && state.position.has_value())
    this->position = motor_to_cover(*state.position);
  if ((fields & FIELD_STATUS) && state.status.has_value() && *state.status != STATUS_MOVING)
    this->current_operation = cover::COVER_OPERATION_IDLE;
  this->publish_state();
}

}  // namespace esphome::dooya_bidir
