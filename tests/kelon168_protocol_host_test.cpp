#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

constexpr uint8_t STATE_LENGTH = 21;
constexpr uint8_t COMMAND_LIGHT = 0x00;
constexpr uint8_t COMMAND_IFEEL = 0x0D;
constexpr uint8_t FOLLOW_ME_ENABLED = 0x80;
constexpr uint8_t MODE_AUTO = 1;

using State = std::array<uint8_t, STATE_LENGTH>;

uint8_t xor_bytes(const State &state, uint8_t start, uint8_t end_exclusive) {
  uint8_t out = 0;
  for (uint8_t i = start; i < end_exclusive; i++)
    out ^= state[i];
  return out;
}

void checksum(State *state) {
  (*state)[13] = xor_bytes(*state, 2, 13);
  (*state)[20] = xor_bytes(*state, 14, 20);
}

bool valid_checksum(const State &state) {
  return state[0] == 0x83 && state[1] == 0x06 && state[13] == xor_bytes(state, 2, 13) &&
         state[20] == xor_bytes(state, 14, 20);
}

State make_default() {
  State state{};
  state[0] = 0x83;
  state[1] = 0x06;
  state[6] = 0x80;
  state[18] = 0x08;
  state[3] = (MODE_AUTO & 0x07) | ((23 - 16) << 4);
  checksum(&state);
  return state;
}

uint8_t reverse_bits(uint8_t value) {
  value = ((value & 0xF0) >> 4) | ((value & 0x0F) << 4);
  value = ((value & 0xCC) >> 2) | ((value & 0x33) << 2);
  value = ((value & 0xAA) >> 1) | ((value & 0x55) << 1);
  return value;
}

State make_follow_me(float temperature, bool enabled) {
  State state = make_default();
  state[11] = enabled ? FOLLOW_ME_ENABLED : 0x00;
  state[12] = static_cast<uint8_t>(std::lround(std::fmin(std::fmax(temperature, 0.0f), 50.0f)));
  state[15] = COMMAND_IFEEL;
  checksum(&state);
  return state;
}

State make_display_off() {
  State state = make_default();
  state[6] |= 0x20;
  state[15] = COMMAND_LIGHT;
  checksum(&state);
  return state;
}

}  // namespace

int main() {
  const auto def = make_default();
  assert(def[0] == 0x83);
  assert(def[1] == 0x06);
  assert(def[3] == 0x71);
  assert(def[6] == 0x80);
  assert(def[18] == 0x08);
  assert(valid_checksum(def));

  const auto follow = make_follow_me(24.4f, true);
  assert(follow[11] == FOLLOW_ME_ENABLED);
  assert(follow[12] == 24);
  assert(follow[15] == COMMAND_IFEEL);
  assert(follow[13] == 0x69);
  assert(follow[20] == 0x05);
  assert(valid_checksum(follow));

  const auto display = make_display_off();
  assert(display[6] == 0xA0);
  assert(display[15] == COMMAND_LIGHT);
  assert(display[13] == 0xD1);
  assert(display[20] == 0x08);
  assert(valid_checksum(display));

  assert(reverse_bits(0x83) == 0xC1);
  assert(reverse_bits(0x06) == 0x60);
  assert(reverse_bits(0xA0) == 0x05);

  auto invalid = display;
  invalid[13] = 0x00;
  assert(!valid_checksum(invalid));

  std::cout << "kelon168 protocol host tests passed\n";
  return 0;
}
