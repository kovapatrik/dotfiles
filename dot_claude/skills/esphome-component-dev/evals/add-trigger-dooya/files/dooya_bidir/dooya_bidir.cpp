#include "dooya_bidir.h"

#include <algorithm>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::dooya_bidir {

static const char *const TAG = "dooya_bidir";

static const char *packet_type_name(PacketType type) {
  switch (type) {
    case PacketType::ONEWAY:
      return "1way";
    case PacketType::TWOWAY:
      return "2way";
    case PacketType::ACK:
      return "ack";
    default:
      return "?";
  }
}

void DooyaBidir::setup() {
  if (this->address_ == 0) {
    // No address configured: use the last 4 bytes of the MAC, so it stays the same across reflashes.
    uint8_t mac[6];
    get_mac_address_raw(mac);
    this->address_ = encode_uint32(mac[2], mac[3], mac[4], mac[5]);
    if (this->address_ == 0xFFFFFFFF)  // reserved (broadcast)
      this->address_ ^= 1;
    this->address_from_mac_ = true;
  }
  if (!this->key_set_)
    key_from_bytes(DOOYA_DEFAULT_KEY, this->key_);
  this->parent_->register_listener(this);
  this->disable_loop();  // only runs while there's something to send or wait for

  // Read every motor once, so entities start with real values
  for (auto *motor : this->motors_)
    motor->refresh();
}

void DooyaBidir::update() {
  for (auto *motor : this->motors_)
    motor->poll(this->get_update_interval());
}

void DooyaBidir::dump_config() {
  ESP_LOGCONFIG(TAG, "Dooya BiDir:");
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Address: %08" PRIX32 "%s", this->address_,
                this->address_from_mac_ ? " (from MAC; set `address: 0x...` to keep it when changing boards)" : "");
  char codes[format_hex_pretty_size(16)];
  ESP_LOGCONFIG(TAG, "  Pair codes: %s",
                format_hex_pretty_to(codes, this->pair_codes_.data(), std::min<size_t>(this->pair_codes_.size(), 16), ' '));
  for (auto *motor : this->motors_)
    ESP_LOGCONFIG(TAG, "  Motor '%s': %08" PRIX32, motor->get_name(), motor->get_address());
}

void DooyaBidir::on_packet(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi) {
  char hex[format_hex_pretty_size(64)];
  ESP_LOGV(TAG, "RX raw (%u B, rssi %.1f, lqi %u): %s", (unsigned) packet.size(), rssi, lqi,
           format_hex_pretty_to(hex, packet.data(), packet.size(), ' '));

  // In variable-length mode the CC1101 strips the length byte (and Dooya's CRC isn't delivered):
  // rebuild [length][body] for decode().
  uint8_t frame[1 + MAX_BODY];
  if (packet.size() > MAX_BODY)
    return;
  frame[0] = static_cast<uint8_t>(packet.size());
  std::copy(packet.begin(), packet.end(), frame + 1);

  Message msg;
  if (!decode(frame, packet.size() + 1, this->key_, msg)) {
    ESP_LOGD(TAG, "RX undecodable packet (%u B)", (unsigned) packet.size());
    return;
  }

  auto *motor = this->get_motor(msg.src);
  // ACKs have no counter
  char counter[5] = "----";
  if (msg.type != PacketType::ACK)
    snprintf(counter, sizeof(counter), "%04X", msg.counter);
  ESP_LOGD(TAG, "RX %08" PRIX32 " (%s) -> %08" PRIX32 " [%s ctr %s grp %04X]: %s", msg.src,
           motor != nullptr ? motor->get_name() : "unknown", msg.dst, packet_type_name(msg.type), counter, msg.groups,
           format_hex_pretty_to(hex, msg.payload.data(), msg.payload.size(), ' '));

  if (msg.dst != this->address_)
    return;

  if (msg.type == PacketType::TWOWAY)
    this->send_ack_(msg);  // immediately, never queued

  uint8_t pair_code;
  if (this->is_accepted_pair_reply_(msg, pair_code)) {
    if (motor != nullptr) {
      ESP_LOGI(TAG, "Pairing reply (code %02X) from motor '%s' (%08" PRIX32 "), already configured", pair_code,
               motor->get_name(), msg.src);
    } else {
      ESP_LOGI(TAG, "Paired new motor %08" PRIX32 " (code %02X). Add it under `motors:` with `address: 0x%08" PRIX32 "`",
               msg.src, pair_code, msg.src);
    }
    this->pair_callback_.call(msg.src, pair_code, motor != nullptr);
  } else if (parse_pair_reply(msg, pair_code) && this->in_flight_.has_value() &&
             this->in_flight_->await == Await::PAIR) {
    ESP_LOGW(TAG, "Possible pairing reply with unknown code %02X from %08" PRIX32 ". If it is one, add 0x%02X to `pair_codes:`",
             pair_code, msg.src, pair_code);
  }

  this->check_in_flight_(msg);

  if (motor != nullptr)
    motor->on_message(msg, rssi);
}

DooyaMotor *DooyaBidir::get_motor(uint32_t address) const {
  for (auto *motor : this->motors_) {
    if (motor->get_address() == address)
      return motor;
  }
  return nullptr;
}

void DooyaMotor::on_message(const Message &msg, float rssi) {
  this->last_seen_ = millis();
  this->seen_ = true;
  this->missed_ = 0;
  this->set_online_(true);

  for (auto *device : this->devices_)
    device->on_message(msg, rssi);

  ReadReply reply;
  if (!parse_read_reply(msg, reply))
    return;
  if (this->dump_ && this->dump_->active && reply.reg == this->dump_->pending) {
    auto &d = *this->dump_;
    for (uint16_t i = 0; i < reply.len && reply.reg + i <= 0xFF; i++) {
      d.data[reply.reg + i] = reply.data[i];
      d.valid[reply.reg + i] = true;
    }
    this->dump_next_();
  }
  const uint8_t fields = apply_read_reply(reply, this->state_);
  if (fields != 0) {
    for (auto *device : this->devices_)
      device->on_state_changed(this->state_, fields);
  }
}

bool DooyaMotor::send_action(DooyaAction action) {
  return this->parent_->send(make_action(this->parent_->get_address(), this->address_, action));
}

bool DooyaMotor::send_move(float position_percent) {
  int pos = static_cast<int>(position_percent + 0.5f);
  return this->parent_->send(make_move(this->parent_->get_address(), this->address_, pos, -1));
}

// The motor jiggles and flips its position scale; read both back
bool DooyaMotor::set_direction(DooyaDirection direction) {
  if (!this->send_action(direction == DIR_REVERSED ? ACT_DIRECTION_REVERSED : ACT_DIRECTION_NORMAL))
    return false;
  this->read(REG_DIRECTION, 1);
  this->read_position();
  return true;
}

// Read back, so the state follows what the motor really did
bool DooyaMotor::set_repeater(bool on) {
  if (!this->send_action(on ? ACT_REPEATER_ON : ACT_REPEATER_OFF))
    return false;
  this->read(REG_RADIO, 1);
  return true;
}

void DooyaMotor::refresh() {
  this->last_polled_ = millis();
  this->polled_ = true;
  this->read_position();
  for (auto *device : this->devices_)
    device->on_refresh();
}

void DooyaMotor::poll(uint32_t interval) {
  // Skip if the motor was heard from, or already asked, within the last half interval
  const uint32_t now = millis();
  const uint32_t recent = interval / 2;
  if ((this->seen_ && now - this->last_seen_ < recent) || (this->polled_ && now - this->last_polled_ < recent))
    return;
  this->last_polled_ = now;
  this->polled_ = true;
  this->read_position();
}

void DooyaMotor::on_no_answer(const TxRequest &req) {
  // A dump read that got no answer: leave those registers empty and continue
  if (this->dump_ && this->dump_->active && req.await == Await::READ_REPLY && req.read_reg == this->dump_->pending)
    this->dump_next_();

  if (this->missed_ < 255)
    this->missed_++;
  if (this->missed_ >= OFFLINE_AFTER)
    this->set_online_(false);
}

static constexpr uint8_t DUMP_READ_LEN = 8;  // the largest read the motor answers reliably

bool DooyaMotor::dump_registers(uint8_t first, uint8_t last) {
  if (first > last)
    return false;
  if (!this->dump_)
    this->dump_ = std::make_unique<RegisterDump>();
  auto &d = *this->dump_;
  if (d.active) {
    ESP_LOGW(TAG, "Register dump of '%s' already running", this->name_);
    return false;
  }
  d.active = true;
  d.first = first;
  d.last = last;
  d.next = first;
  d.valid.reset();
  ESP_LOGI(TAG, "Dumping registers %02X-%02X of '%s' (%u reads, ~0.5 s each)", first, last, this->name_,
           (unsigned) ((last - first) / DUMP_READ_LEN + 1));
  this->dump_next_();
  return true;
}

// Queue the next read, or finish. One read at a time, so the queue never fills up.
void DooyaMotor::dump_next_() {
  auto &d = *this->dump_;
  while (d.next <= d.last && d.valid[d.next])
    d.next++;  // already received (e.g. a longer reply than requested)
  if (d.next > d.last) {
    this->dump_finish_();
    return;
  }
  d.pending = static_cast<uint8_t>(d.next);
  const uint8_t len = static_cast<uint8_t>(std::min<uint16_t>(DUMP_READ_LEN, d.last - d.next + 1));
  d.next += len;
  if (!this->read(d.pending, len)) {
    ESP_LOGW(TAG, "Register dump of '%s' aborted: couldn't queue a read", this->name_);
    d.active = false;
  }
}

void DooyaMotor::dump_finish_() {
  auto &d = *this->dump_;
  d.active = false;

  ESP_LOGI(TAG, "Registers of '%s' (%08" PRIX32 "), %02X-%02X:", this->name_, this->address_, d.first, d.last);
  ESP_LOGI(TAG, "      00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F");
  for (uint16_t row = d.first & 0xF0; row <= d.last; row += 16) {
    char line[16 * 3 + 2]{};
    char *p = line;
    for (uint16_t i = row; i < row + 16; i++) {
      if (i == row + 8)
        *p++ = ' ';
      if (i < d.first || i > d.last) {
        p += snprintf(p, 4, "   ");
      } else if (d.valid[i]) {
        p += snprintf(p, 4, "%02X ", d.data[i]);
      } else {
        p += snprintf(p, 4, "-- ");
      }
    }
    ESP_LOGI(TAG, "  %02X: %s", row, line);
  }

  // Changes since the previous dump (registers read both times)
  unsigned compared = 0, changed = 0;
  for (uint16_t i = d.first; i <= d.last; i++) {
    if (!d.valid[i] || !d.prev_valid[i])
      continue;
    compared++;
    if (d.data[i] != d.prev[i]) {
      ESP_LOGI(TAG, "  changed %02X: %02X -> %02X", i, d.prev[i], d.data[i]);
      changed++;
    }
  }
  if (compared == 0) {
    ESP_LOGI(TAG, "No earlier values for these registers, nothing to compare");
  } else {
    ESP_LOGI(TAG, "%u of %u register(s) changed since the previous dump", changed, compared);
  }
  // Remember this dump, merged into the previous one (so partial dumps compare against the last value seen)
  for (uint16_t i = d.first; i <= d.last; i++) {
    if (d.valid[i]) {
      d.prev[i] = d.data[i];
      d.prev_valid[i] = true;
    }
  }
}

void DooyaMotor::set_online_(bool online) {
  if (this->online_.has_value() && *this->online_ == online)
    return;
  this->online_ = online;
  if (online) {
    ESP_LOGI(TAG, "Motor '%s' (%08" PRIX32 ") is online", this->name_, this->address_);
  } else {
    ESP_LOGW(TAG, "Motor '%s' (%08" PRIX32 ") is not answering", this->name_, this->address_);
  }
  for (auto *device : this->devices_)
    device->on_availability_changed(online);
}

bool DooyaMotor::read(uint8_t reg, uint8_t len) {
  return this->parent_->send(make_read(this->parent_->get_address(), this->address_, len, reg));
}

bool DooyaMotor::read_position() { return this->read(REG_STATUS, 3); }
bool DooyaMotor::read_settings() { return this->read(REG_REWIND, 2); }
bool DooyaMotor::read_model() { return this->read(REG_MODEL, MODEL_LEN); }

bool DooyaBidir::send(uint32_t dst, const std::vector<uint8_t> &payload, bool twoway, bool retry) {
  Message msg;
  msg.type = twoway ? PacketType::TWOWAY : PacketType::ONEWAY;
  msg.dst = dst;
  msg.groups = 1;  // TODO: confirm
  msg.payload = payload;
  return this->send(std::move(msg), retry);
}

// The pair request is addressed to ourselves; the motor in P2 mode replies with ACK 00 03 from its own address.
bool DooyaBidir::pair() {
  ESP_LOGI(TAG, "Sending pair request, the motor must be in pairing mode (P2)");
  return this->send(make_action(this->address_, this->address_, ACT_PAIR));
}

bool DooyaBidir::send(Message msg, bool retry) {
  if (msg.payload.empty())
    return false;  // e.g. make_move() with nothing to do
  if (msg.src == 0)
    msg.src = this->address_;  // exploration may send as another controller
  const uint32_t dst = msg.dst;
  const bool twoway = msg.type == PacketType::TWOWAY;
  const auto &payload = msg.payload;

  TxRequest req;
  req.dst = dst;
  req.max_attempts = retry ? MAX_ATTEMPTS : 1;
  req.frame = encode(msg, this->key_);
  if (req.frame.empty()) {
    ESP_LOGW(TAG, "encode() failed");
    return false;
  }
  char hex[format_hex_pretty_size(MAX_PAYLOAD)];
  ESP_LOGD(TAG, "TX queued -> %08" PRIX32 " [%s ctr %04X grp %04X]: %s", dst, packet_type_name(msg.type), msg.counter,
           msg.groups, format_hex_pretty_to(hex, payload.data(), payload.size(), ' '));
  req.is_move = payload.size() >= 2 && payload[0] == OP_COMMAND && payload[1] == CMD_MOVE;
  if (!twoway) {
    req.await = Await::NOTHING;
  } else if (payload.size() >= 3 && payload[0] == OP_COMMAND && payload[1] == CMD_ACTION && payload[2] == ACT_PAIR) {
    req.await = Await::PAIR;
  } else if (payload.size() >= 3 && payload[0] == OP_READ) {
    req.await = Await::READ_REPLY;
    req.read_reg = payload[2];
  } else {
    req.await = Await::ACK;
  }

  // An identical read already queued (e.g. two entities reading the same registers) is enough
  if (req.await == Await::READ_REPLY) {
    for (const auto &queued : this->queue_) {
      if (queued.frame == req.frame)
        return true;
    }
  }

  // A newer move replaces a queued (not yet sent) move to the same motor
  if (req.is_move) {
    for (auto &queued : this->queue_) {
      if (queued.is_move && queued.dst == dst) {
        queued = std::move(req);
        return true;
      }
    }
  }

  if (this->queue_.size() >= MAX_QUEUE) {
    ESP_LOGW(TAG, "TX queue full, dropping packet to %08" PRIX32, dst);
    return false;
  }
  this->queue_.push_back(std::move(req));
  this->enable_loop();
  return true;
}

void DooyaBidir::loop() {
  if (this->in_flight_.has_value()) {
    auto &req = *this->in_flight_;
    const bool pairing = req.await == Await::PAIR;
    const uint32_t timeout = pairing ? PAIR_TIMEOUT_MS : req.acked ? REPLY_TIMEOUT_MS : ACK_TIMEOUT_MS;
    if (millis() - this->sent_at_ < timeout)
      return;  // still waiting
    if (pairing) {
      ESP_LOGW(TAG, "No pairing reply. Is the motor in pairing mode (P2)?");
      this->finish_in_flight_();
    } else if (req.attempts < req.max_attempts) {
      ESP_LOGD(TAG, "No %s from %08" PRIX32 ", retrying (%u/%u)", req.acked ? "reply" : "ACK", req.dst,
               req.attempts + 1, req.max_attempts);
      req.acked = false;
      this->transmit_in_flight_();
      return;
    } else {
      ESP_LOGW(TAG, "No %s from %08" PRIX32 " after %u attempt(s), giving up", req.acked ? "reply" : "ACK", req.dst,
               req.max_attempts);
      if (auto *motor = this->get_motor(req.dst))
        motor->on_no_answer(req);
      this->finish_in_flight_();
    }
  }

  if (this->queue_.empty()) {
    this->disable_loop();
    return;
  }
  this->in_flight_ = std::move(this->queue_.front());
  this->queue_.pop_front();
  this->transmit_in_flight_();
}

void DooyaBidir::transmit_in_flight_() {
  auto &req = *this->in_flight_;
  req.attempts++;
  if (!this->transmit_frame_(req.frame))
    ESP_LOGW(TAG, "Transmit to %08" PRIX32 " failed", req.dst);
  this->sent_at_ = millis();
  if (req.await == Await::NOTHING)
    this->finish_in_flight_();
}

// Called for every decoded packet addressed to us: does it complete the request in flight?
void DooyaBidir::check_in_flight_(const Message &msg) {
  if (!this->in_flight_.has_value())
    return;
  auto &req = *this->in_flight_;

  // The pair request goes to our own address; the reply comes from the (unknown) motor
  if (req.await == Await::PAIR) {
    uint8_t code;
    if (this->is_accepted_pair_reply_(msg, code))
      this->finish_in_flight_();
    return;
  }
  if (msg.src != req.dst)
    return;

  if (req.await == Await::ACK && msg.type == PacketType::ACK) {
    this->finish_in_flight_();
  } else if (req.await == Await::READ_REPLY) {
    ReadReply reply;
    if (parse_read_reply(msg, reply) && reply.reg == req.read_reg) {
      this->finish_in_flight_();
    } else if (msg.type == PacketType::ACK && !req.acked) {
      req.acked = true;  // now wait up to REPLY_TIMEOUT_MS for the reply
      this->sent_at_ = millis();
    }
  }
}

bool DooyaBidir::is_accepted_pair_reply_(const Message &msg, uint8_t &code) const {
  return parse_pair_reply(msg, code) &&
         std::find(this->pair_codes_.begin(), this->pair_codes_.end(), code) != this->pair_codes_.end();
}

void DooyaBidir::finish_in_flight_() {
  this->in_flight_.reset();
  if (!this->queue_.empty())
    this->enable_loop();
}

void DooyaBidir::send_ack_(const Message &to) {
  auto frame = encode(make_ack(to), this->key_);
  if (frame.empty() || !this->transmit_frame_(frame))
    ESP_LOGW(TAG, "Failed to ACK %08" PRIX32, to.src);
}

// encode() builds the whole frame incl. length byte and CRC. In variable-length mode the CC1101 would add
// its own length byte (counting the CRC too), so send in fixed-length mode and switch back for RX.
bool DooyaBidir::transmit_frame_(const std::vector<uint8_t> &frame) {
  this->parent_->set_packet_length(static_cast<uint8_t>(frame.size()));
  auto err = this->parent_->transmit_packet(frame);
  // transmit_packet() already re-entered RX with the fixed-length settings. Restore PKTLEN to the maximum
  // (in variable-length mode it's the largest accepted packet), switch back to variable length, and
  // re-enter RX through IDLE so the restored settings apply.
  this->parent_->set_packet_length(255);
  this->parent_->set_packet_length(0);
  this->parent_->begin_rx();
  return err == cc1101::CC1101Error::NONE;
}

}  // namespace esphome::dooya_bidir
