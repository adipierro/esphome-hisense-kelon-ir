#include "kelon168_protocol.h"

#include <inttypes.h>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// reference on https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Kelon.cpp

namespace esphome {
namespace hisense_kelon_ir {

static const char *const TAG = "hisense_kelon_ir.protocol";

static const uint16_t KELON168_HDR_MARK = 9000;
static const uint16_t KELON168_HDR_SPACE = 4600;
static const uint16_t KELON168_BIT_MARK = 560;
static const uint16_t KELON168_ONE_SPACE = 1680;
static const uint16_t KELON168_ZERO_SPACE = 600;
static const uint32_t KELON168_FOOTER_SPACE = 8000;
static const uint32_t KELON168_GAP = 20000;
static const uint32_t KELON168_FREQ = 38000;
static const uint8_t KELON168_SECTION1_SIZE = 6;
static const uint8_t KELON168_SECTION2_SIZE = 8;
static const uint8_t KELON168_SECTION3_SIZE = 7;
static const uint8_t KELON168_CHECKSUM_BYTE1 = 13;
static const uint8_t KELON168_CHECKSUM_BYTE2 = KELON168_STATE_LENGTH - 1;

static uint8_t xor_bytes(const uint8_t *data, uint8_t length) {
  uint8_t value = 0;
  for (uint8_t i = 0; i < length; i++)
    value ^= data[i];
  return value;
}

Kelon168Data Kelon168Protocol::make_default() {
  Kelon168Data data;
  data.state.fill(0x00);
  data.state[0] = 0x83;
  data.state[1] = 0x06;
  data.state[6] = 0x80;
  data.state[18] = 0x08;
  data.state[3] = (KELON168_MODE_AUTO & 0x07) | ((23 - 16) << 4);
  checksum(&data);
  return data;
}

bool Kelon168Protocol::valid_checksum(const Kelon168Data &data) {
  if (data.state[0] != 0x83 || data.state[1] != 0x06)
    return false;
  if (data.state[KELON168_CHECKSUM_BYTE1] != xor_bytes(&data.state[2], KELON168_CHECKSUM_BYTE1 - 2))
    return false;
  if (data.state[KELON168_CHECKSUM_BYTE2] !=
      xor_bytes(&data.state[KELON168_CHECKSUM_BYTE1 + 1], KELON168_CHECKSUM_BYTE2 - KELON168_CHECKSUM_BYTE1 - 1))
    return false;
  return true;
}

void Kelon168Protocol::checksum(Kelon168Data *data) {
  data->state[KELON168_CHECKSUM_BYTE1] = xor_bytes(&data->state[2], KELON168_CHECKSUM_BYTE1 - 2);
  data->state[KELON168_CHECKSUM_BYTE2] =
      xor_bytes(&data->state[KELON168_CHECKSUM_BYTE1 + 1], KELON168_CHECKSUM_BYTE2 - KELON168_CHECKSUM_BYTE1 - 1);
}

uint8_t Kelon168Protocol::reverse_bits(uint8_t value) {
  value = ((value & 0xF0) >> 4) | ((value & 0x0F) << 4);
  value = ((value & 0xCC) >> 2) | ((value & 0x33) << 2);
  value = ((value & 0xAA) >> 1) | ((value & 0x55) << 1);
  return value;
}

void Kelon168Protocol::encode(remote_base::RemoteTransmitData *dst, const Kelon168Data &data) {
  dst->set_carrier_frequency(KELON168_FREQ);
  dst->reserve(2 + KELON168_STATE_LENGTH * 16 + 3);
  this->encode_section_(dst, data.state.data(), KELON168_SECTION1_SIZE, true, KELON168_FOOTER_SPACE);
  this->encode_section_(dst, data.state.data() + KELON168_SECTION1_SIZE, KELON168_SECTION2_SIZE, false,
                        KELON168_FOOTER_SPACE);
  this->encode_section_(dst, data.state.data() + KELON168_SECTION1_SIZE + KELON168_SECTION2_SIZE,
                        KELON168_SECTION3_SIZE, false, KELON168_GAP);
}

void Kelon168Protocol::encode_section_(remote_base::RemoteTransmitData *dst, const uint8_t *data, uint8_t size,
                                       bool header, uint32_t footer_space) {
  if (header)
    dst->item(KELON168_HDR_MARK, KELON168_HDR_SPACE);
  for (uint8_t i = 0; i < size; i++) {
    uint8_t byte = data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      dst->item(KELON168_BIT_MARK, (byte & (1 << bit)) ? KELON168_ONE_SPACE : KELON168_ZERO_SPACE);
    }
  }
  dst->item(KELON168_BIT_MARK, footer_space);
}

optional<Kelon168Data> Kelon168Protocol::decode(remote_base::RemoteReceiveData src) {
  Kelon168Data out;
  if (!this->decode_section_(&src, out.state.data(), KELON168_SECTION1_SIZE, true, KELON168_FOOTER_SPACE))
    return {};
  if (!this->decode_section_(&src, out.state.data() + KELON168_SECTION1_SIZE, KELON168_SECTION2_SIZE, false,
                             KELON168_FOOTER_SPACE))
    return {};
  if (!this->decode_section_(&src, out.state.data() + KELON168_SECTION1_SIZE + KELON168_SECTION2_SIZE,
                             KELON168_SECTION3_SIZE, false, KELON168_GAP))
    return {};
  if (!valid_checksum(out))
    return {};
  return out;
}

bool Kelon168Protocol::decode_section_(remote_base::RemoteReceiveData *src, uint8_t *data, uint8_t size, bool header,
                                       uint32_t footer_space) {
  if (header && !src->expect_item(KELON168_HDR_MARK, KELON168_HDR_SPACE))
    return false;

  for (uint8_t i = 0; i < size; i++) {
    uint8_t byte = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (!src->expect_mark(KELON168_BIT_MARK))
        return false;
      if (src->expect_space(KELON168_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!src->expect_space(KELON168_ZERO_SPACE)) {
        return false;
      }
    }
    data[i] = byte;
  }

  return src->expect_pulse_with_gap(KELON168_BIT_MARK, footer_space);
}

void Kelon168Protocol::dump(const Kelon168Data &data) {
  char buffer[KELON168_STATE_LENGTH * 3 + 1];
  size_t pos = 0;
  for (uint8_t i = 0; i < KELON168_STATE_LENGTH; i++) {
    pos += snprintf(buffer + pos, sizeof(buffer) - pos, "%02X%s", data.state[i], i + 1 == KELON168_STATE_LENGTH ? "" : " ");
  }
  ESP_LOGI(TAG, "Received Kelon168: %s (command=0x%02X)", buffer, data.command());
}

}  // namespace hisense_kelon_ir
}  // namespace esphome
