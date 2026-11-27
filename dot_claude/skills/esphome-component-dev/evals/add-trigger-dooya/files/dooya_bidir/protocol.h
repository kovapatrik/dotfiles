#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace esphome::dooya_bidir {

// Frame size limits. Frame = [length:1] [body] [CRC:2]; the length byte counts the body.
static constexpr size_t MAX_FRAME = 64;                                   // CC1101 FIFO size
static constexpr size_t CRC_SIZE = 2;
static constexpr size_t MAX_BODY = MAX_FRAME - 1 - CRC_SIZE;              // 61: largest valid length byte
static constexpr size_t HEADER_SIZE = 2 + 2 + 4 + 4 + 2;                  // flags, counter, src, dst, groups
static constexpr size_t ACK_HEADER_SIZE = HEADER_SIZE - 2;                // ACKs have no counter
static constexpr size_t MAX_PAYLOAD = MAX_BODY - HEADER_SIZE;             // 47

enum DooyaReg : uint8_t {
  REG_STATUS = 0x03,          // 1: 00 idle, 10 moving, 70 stopped by obstacle at bottom
  REG_POSITION = 0x04,        // 2: 8.8 fixed-point percent (0 = open in normal direction)
  REG_MODEL = 0x11,           // 8: ASCII, e.g. "DM45EV/S" (MODEL_LEN)
  REG_ENCODER = 0x21,         // 4: current, centred on 0x50000000
  REG_LIMIT_TOP = 0x25,       // 4: encoder value
  REG_LIMIT_BOTTOM = 0x29,    // 4: encoder value
  REG_FAVOURITE_1 = 0x2D,     // 4: encoder value
  REG_FAVOURITE_2 = 0x31,     // 4: encoder value
  REG_COUNTER = 0x3D,         // 1: changes over time (rolling code?)
  REG_DIRECTION = 0x45,       // 1: DooyaDirection
  REG_FAVOURITE_MASK = 0x46,  // 1: bitmask of set favourites
  REG_REWIND = 0x52,          // 1: 00 off, 01 on (rewind at obstacle)
  REG_SENSITIVITY = 0x53,     // 1: DooyaSensitivity
  REG_RADIO = 0x85,           // 1: bit 3 (REPEATER_BIT) = signal repeater, other bits unknown
};

static constexpr uint8_t REPEATER_BIT = 0x08;

static constexpr uint8_t MODEL_LEN = 8;

enum DooyaStatus : uint8_t {
  STATUS_IDLE = 0x00,
  STATUS_MOVING = 0x10,
  STATUS_OBSTACLE = 0x70,
};

enum DooyaDirection : uint8_t {
  DIR_NORMAL = 0x01,
  DIR_REVERSED = 0x02
};

// Obstacle detection mode (Aluprof EV/Y manual): the remote's P2, Stop, Down cycles through them
enum DooyaSensitivity : uint8_t {
  SENS_MEDIUM = 0x00,  // mode I, factory default: 1 signal
  SENS_HIGH = 0x01,    // mode II: 2 signals
  SENS_LOW = 0x02,     // mode III: 3 signals
};

// Payload opcode: the first payload byte
enum DooyaOpcode : uint8_t {
  OP_STATUS = 0x00,      // 00 = ACK, 00 <pair code> = pairing reply
  OP_COMMAND = 0x02,     // 02 <DooyaCommand> ...
  OP_READ_REPLY = 0x04,  // 04 LEN ADDR data...
  OP_READ = 0x05,        // 05 LEN ADDR
};

// Second payload byte of OP_COMMAND
enum DooyaCommand : uint8_t {
  CMD_BUTTON = 0x02,  // 02 02 <button> 00: remote button press
  CMD_ACTION = 0x03,  // 02 03 <DooyaAction> <param>
  CMD_MOVE = 0x04,    // 02 04 <mode> <position %> <tilt degrees>
};

// CMD_MOVE bits
enum DooyaMove : uint8_t {
    MOVE_TILT = 0x20,
    MOVE_POSITION = 0x40
};

// CMD_ACTION codes
enum DooyaAction : uint8_t {
  ACT_OPEN = 0x01,
  ACT_CLOSE = 0x02,
  ACT_STOP = 0x03,
  ACT_STEP_UP = 0x04,    // 1 %
  ACT_STEP_DOWN = 0x05,  // 1 %
  ACT_FAVOURITE_1 = 0x06,  // go to favourite n
  ACT_FAVOURITE_2 = 0x0B,
  ACT_FAVOURITE_3 = 0x0C,
  ACT_FAVOURITE_4 = 0x0D,
  ACT_PAIR = 0x13,
  ACT_DIRECTION_NORMAL = 0x15,
  ACT_DIRECTION_REVERSED = 0x16,
  ACT_DIRECTION_TOGGLE = 0x17,
  ACT_SAVE_FAVOURITE_2 = 0x18,  // save favourite n at the current position
  ACT_DELETE_FAVOURITE_2 = 0x19,
  ACT_SAVE_FAVOURITE_3 = 0x1A,
  ACT_DELETE_FAVOURITE_3 = 0x1B,
  ACT_SAVE_FAVOURITE_4 = 0x1C,
  ACT_DELETE_FAVOURITE_4 = 0x1D,
  ACT_DELETE_FAVOURITE_1 = 0x26,
  ACT_SAVE_FAVOURITE_1 = 0x27,
  ACT_REPEATER_ON = 0x59,  // signal repeater (REG_RADIO, REPEATER_BIT)
  ACT_REPEATER_OFF = 0x5A,
};

enum class PacketType : uint16_t {
  ONEWAY = 0x12C1,
  TWOWAY = 0x12E1,
  ACK = 0x42C1,     // unencrypted, payload 00
};

enum class Encryption : uint8_t {
  NONE = 0,         // plaintext (e.g. ACK)
  NETWORK_KEY = 1,  // XXTEA with the network key (ONEWAY, TWOWAY)
};

inline Encryption packet_encryption(uint16_t flags) { return static_cast<Encryption>((flags >> 12) & 3); }
inline Encryption packet_encryption(PacketType type) { return packet_encryption(static_cast<uint16_t>(type)); }

// A decoded frame: header + plaintext payload.
struct Message {
  PacketType type{PacketType::ONEWAY};
  uint16_t counter{0};
  uint32_t src{0};
  uint32_t dst{0};
  uint16_t groups{0};
  std::vector<uint8_t> payload;
};

// Frame [length][body][CRC] -> Message. The CRC is optional (checked if present).
// Returns false on CRC error, wrong length, unsupported encryption, or failed sanity check.
bool decode(const uint8_t *frame, size_t len, const uint32_t key[4], Message &out);

// Message -> full frame [length][body][CRC]. Empty on error.
std::vector<uint8_t> encode(const Message &msg, const uint32_t key[4]);

// 16 key bytes -> 4 XXTEA key words (same byte order as the frame data)
void key_from_bytes(const uint8_t bytes[16], uint32_t key[4]);

static constexpr uint8_t DOOYA_DEFAULT_KEY[16] = "DOOYA 20141625\0";

// Standard XXTEA, but using 1 as the base rounds.
static constexpr uint32_t XXTEA_STANDARD_BASE_ROUNDS = 6;
static constexpr uint32_t DOOYA_XXTEA_BASE_ROUNDS = 1;
bool xxtea_encrypt(uint32_t *v, size_t n, const uint32_t key[4], uint32_t base_rounds = DOOYA_XXTEA_BASE_ROUNDS);
bool xxtea_decrypt(uint32_t *v, size_t n, const uint32_t key[4], uint32_t base_rounds = DOOYA_XXTEA_BASE_ROUNDS);

// CRC-16/CMS (poly 0x8005, init 0xFFFF, not reflected, no final XOR).
uint16_t crc16(const uint8_t *data, size_t len);

// Payload builders
Message make_read(uint32_t src_address, uint32_t dst_address, uint8_t len, uint8_t reg_address);
Message make_action(uint32_t src_address, uint32_t dst_address, DooyaAction action, uint8_t param = 0);
Message make_move(uint32_t src_address, uint32_t dst_address, int position_percent, int tilt_degrees);
Message make_ack(const Message &original);

// ---- Incoming messages ----

// Pairing reply from a motor: an ACK-type packet with payload 00 <code>.
// The code differs per motor model (seen: 03, 05, 09); which ones to accept is up to the caller.
bool parse_pair_reply(const Message &msg, uint8_t &code);

// A read reply: `len` register bytes starting at `reg` (copied, independent of the Message).
struct ReadReply {
  uint8_t reg{0};
  uint8_t len{0};
  uint8_t data[MAX_PAYLOAD]{};
};
bool parse_read_reply(const Message &msg, ReadReply &out);

// What we know about a motor. Empty = not reported yet.
struct MotorState {
  std::optional<float> position;  // 0-100 %, 8.8 fixed point (0 = open in normal direction)
  std::optional<DooyaStatus> status;
  std::optional<DooyaDirection> direction;
  std::optional<bool> rewind;
  std::optional<DooyaSensitivity> sensitivity;
  std::optional<bool> repeater;  // signal repeater
  std::optional<std::string> model;  // e.g. "DM45EV/S"
};

// Bits for the fields of MotorState
enum MotorField : uint8_t {
  FIELD_STATUS = 1 << 0,
  FIELD_POSITION = 1 << 1,
  FIELD_DIRECTION = 1 << 2,
  FIELD_REWIND = 1 << 3,
  FIELD_SENSITIVITY = 1 << 4,
  FIELD_MODEL = 1 << 5,
  FIELD_REPEATER = 1 << 6,
};

// Updates every field whose register(s) the reply covers. Returns the MotorField bits it updated (0 = none).
uint8_t apply_read_reply(const ReadReply &reply, MotorState &state);

}  // namespace esphome::dooya_bidir
