#include "protocol.h"

#include <algorithm>
#include <bit>
#include <cstring>

namespace esphome::dooya_bidir {

static constexpr uint32_t XXTEA_DELTA = 0x9e3779b9;

static inline uint32_t xxtea_mx(uint32_t y, uint32_t z, uint32_t sum, size_t p, uint32_t e, const uint32_t key[4]) {
  return (((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ ((sum ^ y) + (key[(p & 3) ^ e] ^ z));
}

bool xxtea_encrypt(uint32_t *v, size_t n, const uint32_t key[4], uint32_t base_rounds) {
  if (n < 2)
    return false;
  uint32_t rounds = base_rounds + 52 / n;
  uint32_t sum = 0;
  uint32_t z = v[n - 1];
  uint32_t y;
  while (rounds-- > 0) {
    sum += XXTEA_DELTA;
    uint32_t e = (sum >> 2) & 3;
    size_t p;
    for (p = 0; p < n - 1; p++) {
      y = v[p + 1];
      z = v[p] += xxtea_mx(y, z, sum, p, e, key);
    }
    y = v[0];
    z = v[n - 1] += xxtea_mx(y, z, sum, p, e, key);
  }
  return true;
}

bool xxtea_decrypt(uint32_t *v, size_t n, const uint32_t key[4], uint32_t base_rounds) {
  if (n < 2)
    return false;
  uint32_t rounds = base_rounds + 52 / n;
  uint32_t sum = rounds * XXTEA_DELTA;
  uint32_t y = v[0];
  uint32_t z;
  while (rounds-- > 0) {
    uint32_t e = (sum >> 2) & 3;
    size_t p;
    for (p = n - 1; p > 0; p--) {
      z = v[p - 1];
      y = v[p] -= xxtea_mx(y, z, sum, p, e, key);
    }
    z = v[n - 1];
    y = v[0] -= xxtea_mx(y, z, sum, p, e, key);
    sum -= XXTEA_DELTA;
  }
  return true;
}

static constexpr uint16_t CRC16_POLY = 0x8005;
static constexpr uint16_t CRC16_INIT = 0xFFFF;

uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = CRC16_INIT;
  for (size_t i = 0; i < len; i++) {
    crc ^= static_cast<uint16_t>(data[i] << 8);
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ CRC16_POLY) : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

// ---- Framing -----------------------------------------------------------------------
// Frame: [length] [flags:2] [counter:2, not in ACKs] [src:4] [dst:4] [groups:2] [payload:N] [CRC:2]
//  - length = number of bytes after the length byte, without the CRC
//  - encrypted (if the flags say so): whole 32-bit words from offset 3; the 0-3 bytes left over stay plaintext
//  - CRC-16/CMS over everything after the length byte
// Confirmed on captured remote frames: header fields big-endian, XXTEA words/key little-endian, base rounds 1,
// length byte meaning, no extra bytes after the payload. Unverified: CRC byte order (assumed big-endian).

static constexpr size_t ENCRYPT_OFFSET = 3;  // length byte + flags stay readable

static void put_u16(std::vector<uint8_t> &out, uint16_t v) {
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

static void put_u32(std::vector<uint8_t> &out, uint32_t v) {
  for (int i = 3; i >= 0; i--)
    out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

static uint16_t get_u16(const uint8_t *p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }

static uint32_t get_u32(const uint8_t *p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

// XXTEA words are the frame bytes reinterpreted as uint32_t on a little-endian CPU.
static_assert(std::endian::native == std::endian::little, "XXTEA word layout assumes a little-endian CPU");

void key_from_bytes(const uint8_t bytes[16], uint32_t key[4]) { std::memcpy(key, bytes, 16); }

static bool has_counter(PacketType type) { return type != PacketType::ACK; }

std::vector<uint8_t> encode(const Message &msg, const uint32_t key[4]) {
  const size_t header = 1 + (has_counter(msg.type) ? HEADER_SIZE : ACK_HEADER_SIZE);
  if (msg.payload.size() > MAX_PAYLOAD)
    return {};
  const size_t total = header + msg.payload.size() + CRC_SIZE;

  std::vector<uint8_t> out;
  out.reserve(total);
  out.push_back(0);  // length, filled in below
  put_u16(out, static_cast<uint16_t>(msg.type));
  if (has_counter(msg.type))
    put_u16(out, msg.counter);
  put_u32(out, msg.src);
  put_u32(out, msg.dst);
  put_u16(out, msg.groups);
  out.insert(out.end(), msg.payload.begin(), msg.payload.end());

  switch (packet_encryption(msg.type)) {
    case Encryption::NONE:
      break;
    case Encryption::NETWORK_KEY: {
      // whole 4-byte words from offset 3; the 0-3 bytes left over stay plaintext
      uint32_t words[MAX_FRAME / 4];
      size_t nwords = (out.size() - ENCRYPT_OFFSET) / 4;
      std::memcpy(words, out.data() + ENCRYPT_OFFSET, nwords * 4);
      if (!xxtea_encrypt(words, nwords, key))
        return {};
      std::memcpy(out.data() + ENCRYPT_OFFSET, words, nwords * 4);
      break;
    }
    default:
      return {};  // unsupported encryption
  }

  out[0] = static_cast<uint8_t>(out.size() - 1);
  uint16_t crc = crc16(out.data() + 1, out.size() - 1);
  out.push_back(static_cast<uint8_t>(crc >> 8));
  out.push_back(static_cast<uint8_t>(crc));
  return out;
}

bool decode(const uint8_t *frame, size_t len, const uint32_t key[4], Message &out) {
  if (len < 1 || len > MAX_FRAME)
    return false;
  const size_t body_len = frame[0];  // bytes after the length byte, without CRC
  if (body_len > MAX_BODY)
    return false;
  const size_t end = 1 + body_len;

  // With CRC (full frame) or without (the CC1101 stops after `length` bytes in variable-length mode)
  if (len == end + CRC_SIZE) {
    uint16_t crc = static_cast<uint16_t>((frame[end] << 8) | frame[end + 1]);
    if (crc16(frame + 1, body_len) != crc)
      return false;
  } else if (len != end) {
    return false;
  }

  if (end < 3)
    return false;
  const uint16_t flags = get_u16(frame + 1);
  const PacketType type = static_cast<PacketType>(flags);
  const size_t header = 1 + (has_counter(type) ? HEADER_SIZE : ACK_HEADER_SIZE);
  if (end < header)
    return false;

  uint8_t buf[MAX_FRAME];
  std::copy(frame, frame + end, buf);

  switch (packet_encryption(flags)) {
    case Encryption::NONE:
      break;
    case Encryption::NETWORK_KEY: {
      uint32_t words[MAX_FRAME / 4];
      size_t nwords = (end - ENCRYPT_OFFSET) / 4;
      std::memcpy(words, buf + ENCRYPT_OFFSET, nwords * 4);
      if (!xxtea_decrypt(words, nwords, key))
        return false;
      std::memcpy(buf + ENCRYPT_OFFSET, words, nwords * 4);
      break;
    }
    default:
      return false;  // unsupported encryption
  }

  const size_t payload_len = end - header;

  size_t pos = 3;
  out.type = type;
  out.counter = 0;
  if (has_counter(type)) {
    out.counter = get_u16(buf + pos);
    pos += 2;
  }
  out.src = get_u32(buf + pos);
  pos += 4;
  out.dst = get_u32(buf + pos);
  pos += 4;
  out.groups = get_u16(buf + pos);
  pos += 2;
  out.payload.assign(buf + pos, buf + pos + payload_len);
  return true;
}

Message make_read(uint32_t src_address, uint32_t dst_address, uint8_t len, uint8_t reg_address) {
    Message msg = {
        .type = PacketType::TWOWAY,
        .counter = 0,
        .src = src_address,
        .dst = dst_address,
        .groups = 1,
        .payload = {OP_READ, len, reg_address}
    };
    return msg;
}

Message make_action(uint32_t src_address, uint32_t dst_address, DooyaAction action, uint8_t param) {
    Message msg = {
        .type      = PacketType::TWOWAY,
        .counter    = 0,
        .src        = src_address,
        .dst        = dst_address,
        .groups     = 1,
        .payload    = {OP_COMMAND, CMD_ACTION, action, param},
    };
    return msg;
}

Message make_move(uint32_t src_address, uint32_t dst_address, int position_percent, int tilt_degrees) {
  uint8_t mode = 0;
  if (position_percent >= 0)
    mode |= MOVE_POSITION;
  if (tilt_degrees >= 0)
    mode |= MOVE_TILT;
  if (!mode)
    return {};

  if (position_percent > 100)
    position_percent = 100;
  if (tilt_degrees > 180)
    tilt_degrees = 180;

  Message msg = {
      .type = PacketType::TWOWAY,
      .counter = 0,
      .src = src_address,
      .dst = dst_address,
      .groups = 1,
      .payload = {OP_COMMAND, CMD_MOVE, mode, (uint8_t) (position_percent < 0 ? 0 : position_percent),
                  (uint8_t) (tilt_degrees < 0 ? 0 : tilt_degrees)},
  };
  return msg;
}

Message make_ack(const Message &original) {
  Message msg = {
      .type = PacketType::ACK,
      .src = original.dst,
      .dst = original.src,
      .groups = original.groups,
      .payload = {0},
  };
  return msg;
}

bool parse_pair_reply(const Message &msg, uint8_t &code) {
  if (msg.type != PacketType::ACK || msg.payload.size() != 2 || msg.payload[0] != OP_STATUS)
    return false;
  code = msg.payload[1];
  return true;
}

bool parse_read_reply(const Message &msg, ReadReply &out) {
  const auto &p = msg.payload;
  if (p.size() < 3 || p[0] != OP_READ_REPLY)
    return false;
  const uint8_t len = p[1];
  if (len == 0 || len > MAX_PAYLOAD || p.size() < 3u + len)
    return false;
  out.reg = p[2];
  out.len = len;
  std::copy(p.begin() + 3, p.begin() + 3 + len, out.data);
  return true;
}

uint8_t apply_read_reply(const ReadReply &reply, MotorState &state) {
  // true if registers [reg, reg + n) are all inside the reply
  auto covers = [&](uint8_t reg, uint8_t n) { return reg >= reply.reg && reg + n <= reply.reg + reply.len; };
  auto at = [&](uint8_t reg) { return reply.data[reg - reply.reg]; };
  uint8_t updated = 0;

  if (covers(REG_STATUS, 1)) {
    state.status = static_cast<DooyaStatus>(at(REG_STATUS));
    updated |= FIELD_STATUS;
  }
  if (covers(REG_POSITION, 2)) {
    state.position = at(REG_POSITION) + at(REG_POSITION + 1) / 256.0f;
    updated |= FIELD_POSITION;
  }
  if (covers(REG_DIRECTION, 1)) {
    state.direction = static_cast<DooyaDirection>(at(REG_DIRECTION));
    updated |= FIELD_DIRECTION;
  }
  if (covers(REG_REWIND, 1)) {
    state.rewind = at(REG_REWIND) != 0;
    updated |= FIELD_REWIND;
  }
  if (covers(REG_SENSITIVITY, 1)) {
    state.sensitivity = static_cast<DooyaSensitivity>(at(REG_SENSITIVITY));
    updated |= FIELD_SENSITIVITY;
  }
  if (covers(REG_RADIO, 1)) {
    state.repeater = (at(REG_RADIO) & REPEATER_BIT) != 0;
    updated |= FIELD_REPEATER;
  }
  if (covers(REG_MODEL, MODEL_LEN)) {
    // ASCII, padded with NULs or spaces
    std::string model(reinterpret_cast<const char *>(&reply.data[REG_MODEL - reply.reg]), MODEL_LEN);
    model.erase(std::find(model.begin(), model.end(), '\0'), model.end());
    while (!model.empty() && model.back() == ' ')
      model.pop_back();
    state.model = model;
    updated |= FIELD_MODEL;
  }
  return updated;
}

}  // namespace esphome::dooya_bidir
