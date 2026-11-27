#pragma once

#include <bitset>
#include <deque>
#include <memory>
#include <vector>

#include "esphome/components/cc1101/cc1101.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "protocol.h"

namespace esphome::dooya_bidir {

class DooyaBidir;
class DooyaMotor;

// Anything bound to one motor (cover, sensor, select...) implements this
// and registers itself with its motor.
class DooyaDevice {
 public:
  void set_motor(DooyaMotor *motor) { this->motor_ = motor; }
  // Every message from the motor (optional)
  virtual void on_message(const Message &msg, float rssi) {}
  // A reply updated the motor's state; `fields` = the MotorField bits that were updated
  virtual void on_state_changed(const MotorState &state, uint8_t fields) {}
  // The motor is being (re)read, at boot: queue reads for the registers this device needs (position is always read)
  virtual void on_refresh() {}
  // The motor started or stopped answering
  virtual void on_availability_changed(bool online) {}

 protected:
  DooyaMotor *motor_{nullptr};
};

// What completes a queued request
enum class Await : uint8_t {
  NOTHING,     // one-way: done once sent
  ACK,         // two-way command: done when the motor ACKs
  READ_REPLY,  // read: done when the matching 04 LEN ADDR reply arrives
  PAIR,        // pair request: done when any motor sends a pairing reply (ACK 00 03)
};

struct TxRequest {
  uint32_t dst{0};
  std::vector<uint8_t> frame;  // encoded once, resent as-is on retry
  Await await{Await::NOTHING};
  uint8_t read_reg{0};      // for READ_REPLY
  bool is_move{false};      // queued moves to the same motor are replaced by newer ones
  uint8_t attempts{0};
  uint8_t max_attempts{1};
  bool acked{false};        // READ_REPLY: ACK seen, now waiting for the reply
};

// One paired motor/blind: address + display name, forwards messages to its entities.
class DooyaMotor : public Parented<DooyaBidir> {
 public:
  void set_name(const char *name) { this->name_ = name; }
  const char *get_name() const { return this->name_; }
  void set_address(uint32_t address) { this->address_ = address; }
  uint32_t get_address() const { return this->address_; }
  void add_device(DooyaDevice *device) { this->devices_.push_back(device); }

  void on_message(const Message &msg, float rssi);
  const MotorState &get_state() const { return this->state_; }

  // Commands (queued by the bridge)
  bool send_action(DooyaAction action);
  bool send_move(float position_percent);  // 0 = open, 100 = closed
  bool set_direction(DooyaDirection direction);
  bool set_repeater(bool on);
  bool read(uint8_t reg, uint8_t len);
  bool read_position();                    // status + position (0x03-0x05)
  bool read_settings();                    // rewind + sensitivity (0x52-0x53)
  bool read_model();                       // model name (0x11-0x18)

  // Read position + whatever the devices need (at boot)
  void refresh();
  // Periodic poll from the bridge: reads the position unless the motor was heard from recently
  void poll(uint32_t interval);
  // The bridge gave up on a request to this motor (no answer after all attempts)
  void on_no_answer(const TxRequest &req);

  // Read registers [first, last] (8 at a time, one after another) and log them as a hex table,
  // together with the registers that changed since the previous dump.
  bool dump_registers(uint8_t first = 0x00, uint8_t last = 0xFF);
  // Empty until the motor answered or failed to answer
  optional<bool> is_online() const { return this->online_; }

  static constexpr uint8_t OFFLINE_AFTER = 2;  // failed requests in a row (each already retried)

 protected:
  void set_online_(bool online);
  void dump_next_();
  void dump_finish_();

  // Register dump state, allocated on the first dump
  struct RegisterDump {
    uint8_t data[256]{};
    std::bitset<256> valid;
    uint8_t prev[256]{};
    std::bitset<256> prev_valid;
    bool active{false};
    uint16_t first{0}, last{0}, next{0};  // next register to read
    uint8_t pending{0};                   // register of the read in flight
  };
  std::unique_ptr<RegisterDump> dump_;

  const char *name_{""};
  uint32_t address_{0};
  MotorState state_;
  std::vector<DooyaDevice *> devices_;

  optional<bool> online_;
  uint8_t missed_{0};          // failed requests in a row
  uint32_t last_seen_{0};      // last message from the motor
  bool seen_{false};
  uint32_t last_polled_{0};    // last refresh/poll request
  bool polled_{false};
};

class DooyaBidir : public PollingComponent, public cc1101::CC1101Listener, public Parented<cc1101::CC1101Component> {
 public:
  void setup() override;
  void loop() override;
  void update() override;  // polls every motor
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // cc1101::CC1101Listener
  void on_packet(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi) override;

  void set_address(uint32_t address) { this->address_ = address; }
  uint32_t get_address() const { return this->address_; }
  void set_key(const uint8_t key[16]) {
    key_from_bytes(key, this->key_);
    this->key_set_ = true;
  }
  void register_motor(DooyaMotor *motor) { this->motors_.push_back(motor); }
  DooyaMotor *get_motor(uint32_t address) const;

  // Queue a message (src is set to this bridge's address). Returns false if it can't be encoded or the queue is full.
  // retry = false: send once even if no ACK/reply arrives (for commands that mustn't be repeated)
  bool send(Message msg, bool retry = true);
  // Same, from a raw payload
  bool send(uint32_t dst, const std::vector<uint8_t> &payload, bool twoway = true, bool retry = true);
  // Pair with a motor that is in pairing mode (P2). The reply is logged with the motor's address.
  bool pair();
  // Pairing reply codes (payload 00 <code>) to accept; they differ per motor model
  void set_pair_codes(std::vector<uint8_t> codes) { this->pair_codes_ = std::move(codes); }
  // Called for every accepted pairing reply: (motor address, pair code, already listed under `motors:`)
  template<typename F> void add_on_pair_callback(F &&callback) { this->pair_callback_.add(std::forward<F>(callback)); }

  static constexpr uint32_t ACK_TIMEOUT_MS = 150;     // measured: ~25 ms
  static constexpr uint32_t REPLY_TIMEOUT_MS = 1500;  // measured: ~500 ms
  static constexpr uint32_t PAIR_TIMEOUT_MS = 5000;
  static constexpr uint8_t MAX_ATTEMPTS = 3;  // pair requests are sent only once
  static constexpr size_t MAX_QUEUE = 16;

 protected:
  void send_ack_(const Message &to);
  bool transmit_frame_(const std::vector<uint8_t> &frame);
  void transmit_in_flight_();
  void check_in_flight_(const Message &msg);
  void finish_in_flight_();
  bool is_accepted_pair_reply_(const Message &msg, uint8_t &code) const;

  uint32_t address_{0};  // 0 = not configured, derived from MAC in setup()
  uint32_t key_[4]{};    // XXTEA key words, set from DOOYA_DEFAULT_KEY in setup() unless configured
  bool key_set_{false};
  bool address_from_mac_{false};
  std::vector<DooyaMotor *> motors_;
  std::vector<uint8_t> pair_codes_{0x03, 0x05, 0x09};
  CallbackManager<void(uint32_t, uint8_t, bool)> pair_callback_;

  std::deque<TxRequest> queue_;
  optional<TxRequest> in_flight_;
  uint32_t sent_at_{0};
};

}  // namespace esphome::dooya_bidir
