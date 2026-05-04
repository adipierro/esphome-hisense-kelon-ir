#include "hisense_kelon_ir.h"

#include <cmath>
#include <inttypes.h>

#include "esphome/core/log.h"

namespace esphome {
namespace hisense_kelon_ir {

static const char *const TAG = "hisense_kelon_ir.climate";
static const uint8_t KELON168_FOLLOW_ME_ENABLED = 0x80;

static void log_kelon168_data(const char *prefix, const Kelon168Data &data) {
  char buffer[KELON168_STATE_LENGTH * 3 + 1];
  size_t pos = 0;
  for (uint8_t i = 0; i < KELON168_STATE_LENGTH; i++) {
    pos += snprintf(buffer + pos, sizeof(buffer) - pos, "%02X%s", data.state[i],
                    i + 1 == KELON168_STATE_LENGTH ? "" : " ");
  }
  ESP_LOGI(TAG, "%s Kelon168: %s (command=0x%02X)", prefix, buffer, data.command());
}

void HisenseKelonIRClimate::setup() {
  const bool restored = this->restore_state_().has_value();
  climate_ir::ClimateIR::setup();
  this->power_known_ = restored;
  this->last_tx_ = this->build_state_(
      this->mode, this->target_temperature, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO), this->swing_mode,
      this->preset.value_or(climate::CLIMATE_PRESET_NONE), false, KELON168_COMMAND_MODE);
  this->have_last_tx_ = true;
  if (restored && this->ensure_power_on_boot_ && this->ensure_power_ != ENSURE_POWER_NONE &&
      this->mode != climate::CLIMATE_MODE_OFF) {
    this->set_timeout("ensure_power_on_boot", 1000, [this]() {
      ESP_LOGI(TAG, "Ensuring restored non-off state after boot");
      this->should_ensure_power_ = true;
      this->transmit_state();
      this->should_ensure_power_ = false;
      this->power_known_ = true;
      this->publish_state();
    });
  }
}

void HisenseKelonIRClimate::dump_config() {
  climate_ir::ClimateIR::dump_config();
  const char *ensure = "smart";
  if (this->ensure_power_ == ENSURE_POWER_SUPER)
    ensure = "super";
  else if (this->ensure_power_ == ENSURE_POWER_NONE)
    ensure = "none";
  ESP_LOGCONFIG(TAG, "  Ensure Power: %s", ensure);
  ESP_LOGCONFIG(TAG, "  Ensure Power On Boot: %s", YESNO(this->ensure_power_on_boot_));
}

void HisenseKelonIRClimate::control(const climate::ClimateCall &call) {
  const bool was_on = this->mode != climate::CLIMATE_MODE_OFF;
  const bool was_known = this->power_known_;
  uint8_t command = KELON168_COMMAND_MODE;

  auto mode = call.get_mode();
  if (mode.has_value()) {
    this->mode = *mode;
    command = *mode == climate::CLIMATE_MODE_OFF ? KELON168_COMMAND_POWER : KELON168_COMMAND_MODE;
  }
  auto target_temperature = call.get_target_temperature();
  if (target_temperature.has_value()) {
    this->target_temperature = *target_temperature;
    if (!mode.has_value())
      command = KELON168_COMMAND_TEMP;
  }
  auto fan_mode = call.get_fan_mode();
  if (fan_mode.has_value()) {
    this->fan_mode = fan_mode;
    if (!mode.has_value() && !target_temperature.has_value())
      command = KELON168_COMMAND_FAN_SPEED;
  }
  auto swing_mode = call.get_swing_mode();
  if (swing_mode.has_value()) {
    this->swing_mode = *swing_mode;
    if (!mode.has_value() && !target_temperature.has_value() && !fan_mode.has_value()) {
      command = *swing_mode == climate::CLIMATE_SWING_HORIZONTAL || *swing_mode == climate::CLIMATE_SWING_BOTH
                    ? KELON168_COMMAND_HORIZONTAL_SWING
                    : KELON168_COMMAND_SWING;
    }
  }
  auto preset = call.get_preset();
  if (preset.has_value()) {
    this->preset = preset;
    if (!mode.has_value() && !target_temperature.has_value() && !fan_mode.has_value() && !swing_mode.has_value()) {
      command = *preset == climate::CLIMATE_PRESET_SLEEP    ? KELON168_COMMAND_SLEEP
                : *preset == climate::CLIMATE_PRESET_BOOST ? KELON168_COMMAND_SUPER
                                                            : KELON168_COMMAND_MODE;
    }
  }

  const bool will_be_on = this->mode != climate::CLIMATE_MODE_OFF;
  this->should_ensure_power_ =
      this->ensure_power_ != ENSURE_POWER_NONE && (!was_known || was_on != will_be_on);
  this->next_command_ = command;
  if (was_on != will_be_on) {
    this->follow_me_enabled_ = false;
    this->follow_me_temperature_ = 0;
  }

  if (!will_be_on && !was_on) {
    ESP_LOGD(TAG, "Climate is off; storing preference change without sending IR");
    this->publish_state();
    return;
  }

  this->transmit_state();
  this->should_ensure_power_ = false;
  this->power_known_ = true;
  this->publish_state();
}

void HisenseKelonIRClimate::transmit_state() {
  const auto fan = this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO);
  const auto preset = this->preset.value_or(climate::CLIMATE_PRESET_NONE);

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    if (this->should_ensure_power_)
      this->ensure_power_on_();
    auto off = this->build_state_(climate::CLIMATE_MODE_OFF, this->target_temperature, fan, this->swing_mode, preset, true,
                                  KELON168_COMMAND_POWER);
    this->transmit_kelon_(off);
    return;
  }

  if (this->should_ensure_power_)
    this->ensure_power_on_();

  auto command = this->next_command_;
  if (preset == climate::CLIMATE_PRESET_SLEEP && command == KELON168_COMMAND_MODE)
    command = KELON168_COMMAND_SLEEP;
  else if (preset == climate::CLIMATE_PRESET_BOOST && command == KELON168_COMMAND_MODE)
    command = KELON168_COMMAND_SUPER;

  auto state = this->build_state_(this->mode, this->target_temperature, fan, this->swing_mode, preset, false, command);
  this->transmit_kelon_(state);
}

void HisenseKelonIRClimate::send_follow_me(float temperature, bool enabled) {
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    ESP_LOGD(TAG, "Skipping follow-me command because climate is off");
    return;
  }

  if (!std::isfinite(temperature)) {
    ESP_LOGW(TAG, "Skipping follow-me command because temperature is unavailable");
    return;
  }

  auto data = this->have_last_tx_ ? this->last_tx_ : Kelon168Protocol::make_default();
  const bool state_changed = this->follow_me_enabled_ != enabled;
  this->follow_me_enabled_ = enabled;
  this->follow_me_temperature_ = static_cast<uint8_t>(lroundf(clamp(temperature, 0.0f, 50.0f)));
  data.state[6] &= ~0x20;
  data.state[11] = enabled ? KELON168_FOLLOW_ME_ENABLED : 0x00;
  data.state[12] = this->follow_me_temperature_;
  data.state[15] = state_changed ? KELON168_COMMAND_IFEEL : KELON168_COMMAND_LIGHT;
  Kelon168Protocol::checksum(&data);
  this->transmit_kelon_(data);
}

void HisenseKelonIRClimate::send_display_off() {
  auto data = this->have_last_tx_ ? this->last_tx_ : Kelon168Protocol::make_default();
  data.state[6] |= 0x20;
  data.state[15] = KELON168_COMMAND_LIGHT;
  Kelon168Protocol::checksum(&data);
  this->transmit_kelon_(data, false);
}

bool HisenseKelonIRClimate::on_receive(remote_base::RemoteReceiveData data) {
  auto decoded = Kelon168Protocol().decode(data);
  if (!decoded.has_value())
    return false;

  ESP_LOGD(TAG, "Decoded Kelon168 command 0x%02X", decoded->command());
  this->log_changed_bytes_(*decoded);
  this->apply_received_state_(*decoded);
  return true;
}

void HisenseKelonIRClimate::apply_received_state_(const Kelon168Data &data) {
  const uint8_t command = data.command();
  if (command == KELON168_COMMAND_LIGHT || command == KELON168_COMMAND_IFEEL) {
    const bool follow_me_frame = command == KELON168_COMMAND_IFEEL || (data.state[11] & KELON168_FOLLOW_ME_ENABLED) != 0;
    if (follow_me_frame) {
      this->follow_me_enabled_ = (data.state[11] & KELON168_FOLLOW_ME_ENABLED) != 0;
      this->follow_me_temperature_ = data.state[12];
      this->last_tx_ = data;
      this->last_tx_.state[6] &= ~0x20;
      Kelon168Protocol::checksum(&this->last_tx_);
      this->have_last_tx_ = true;
    }
    ESP_LOGD(TAG, "Ignoring non-climate Kelon168 command 0x%02X for climate state", command);
    return;
  }

  if (data.state[2] & 0x04) {
    if (this->power_known_) {
      this->mode = this->mode == climate::CLIMATE_MODE_OFF ? climate::CLIMATE_MODE_HEAT_COOL : climate::CLIMATE_MODE_OFF;
      this->follow_me_enabled_ = false;
      this->follow_me_temperature_ = 0;
    } else {
      ESP_LOGW(TAG, "Received power-toggle frame before power state was known; leaving climate mode unchanged");
    }
  } else if (this->mode != climate::CLIMATE_MODE_OFF) {
    this->mode = this->decode_mode_(data.state[3] & 0x07);
  }

  if (this->mode != climate::CLIMATE_MODE_OFF) {
    this->mode = this->decode_mode_(data.state[3] & 0x07);
    if (this->mode != climate::CLIMATE_MODE_FAN_ONLY)
      this->target_temperature = 16.0f + ((data.state[3] >> 4) & 0x0F);
  }

  this->fan_mode = this->decode_fan_(data);
  const bool vertical_swing = (data.state[2] & 0x80) && (data.state[8] & 0x40);
  const bool horizontal_swing = data.state[8] & 0x80;
  if (vertical_swing && horizontal_swing)
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  else if (vertical_swing)
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  else if (horizontal_swing)
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  else
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  if ((data.state[5] & 0x90) == 0x90) {
    this->preset = climate::CLIMATE_PRESET_BOOST;
  } else if (data.state[2] & 0x08) {
    this->preset = climate::CLIMATE_PRESET_SLEEP;
  } else {
    this->preset = climate::CLIMATE_PRESET_NONE;
  }

  this->last_tx_ = data;
  this->have_last_tx_ = true;
  this->power_known_ = true;
  this->publish_state();
}

Kelon168Data HisenseKelonIRClimate::build_state_(climate::ClimateMode mode, float target_temperature,
                                                 climate::ClimateFanMode fan_mode,
                                                 climate::ClimateSwingMode swing_mode,
                                                 climate::ClimatePreset preset, bool power_toggle,
                                                 uint8_t command) const {
  auto data = Kelon168Protocol::make_default();
  const uint8_t native_mode = this->encode_mode_(mode);
  const uint8_t temp = static_cast<uint8_t>(lroundf(clamp(target_temperature, 16.0f, 32.0f)));

  data.state[2] = power_toggle ? 0x04 : 0x00;
  if (preset == climate::CLIMATE_PRESET_SLEEP)
    data.state[2] |= 0x08;
  if (swing_mode == climate::CLIMATE_SWING_VERTICAL || swing_mode == climate::CLIMATE_SWING_BOTH)
    data.state[2] |= 0x80;

  this->set_fan_(&data, fan_mode);
  data.state[3] = (native_mode & 0x07) | ((temp - 16) << 4);

  if (preset == climate::CLIMATE_PRESET_BOOST)
    data.state[5] |= 0x90;
  if (swing_mode == climate::CLIMATE_SWING_VERTICAL || swing_mode == climate::CLIMATE_SWING_BOTH)
    data.state[8] |= 0x40;
  if (swing_mode == climate::CLIMATE_SWING_HORIZONTAL || swing_mode == climate::CLIMATE_SWING_BOTH)
    data.state[8] |= 0x80;

  data.state[15] = command;
  this->apply_follow_me_(&data);
  Kelon168Protocol::checksum(&data);
  return data;
}

void HisenseKelonIRClimate::transmit_kelon_(Kelon168Data data, bool remember) {
  Kelon168Protocol::checksum(&data);
  auto transmit = this->transmitter_->transmit();
  Kelon168Protocol().encode(transmit.get_data(), data);
  log_kelon168_data("Sending", data);
  transmit.perform();
  if (remember) {
    this->last_tx_ = data;
    this->have_last_tx_ = true;
  }
}

void HisenseKelonIRClimate::ensure_power_on_() {
  auto mode = this->ensure_power_ == ENSURE_POWER_SUPER ? climate::CLIMATE_MODE_COOL : climate::CLIMATE_MODE_HEAT_COOL;
  auto preset = this->ensure_power_ == ENSURE_POWER_SUPER ? climate::CLIMATE_PRESET_BOOST : climate::CLIMATE_PRESET_NONE;
  auto command = this->ensure_power_ == ENSURE_POWER_SUPER ? KELON168_COMMAND_SUPER : KELON168_COMMAND_IFEEL;
  auto data = this->build_state_(mode, this->ensure_power_ == ENSURE_POWER_SUPER ? 16.0f : 23.0f, climate::CLIMATE_FAN_AUTO,
                                 climate::CLIMATE_SWING_OFF, preset, false, command);
  this->transmit_kelon_(data);
}

void HisenseKelonIRClimate::apply_follow_me_(Kelon168Data *data) const {
  data->state[11] = this->follow_me_enabled_ ? KELON168_FOLLOW_ME_ENABLED : 0x00;
  data->state[12] = this->follow_me_enabled_ ? this->follow_me_temperature_ : 0x00;
}

uint8_t HisenseKelonIRClimate::encode_mode_(climate::ClimateMode mode) const {
  switch (mode) {
    case climate::CLIMATE_MODE_HEAT:
      return KELON168_MODE_HEAT;
    case climate::CLIMATE_MODE_COOL:
      return KELON168_MODE_COOL;
    case climate::CLIMATE_MODE_DRY:
      return KELON168_MODE_DRY;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return KELON168_MODE_FAN;
    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      return KELON168_MODE_AUTO;
  }
}

climate::ClimateMode HisenseKelonIRClimate::decode_mode_(uint8_t mode) const {
  switch (mode) {
    case KELON168_MODE_HEAT:
      return climate::CLIMATE_MODE_HEAT;
    case KELON168_MODE_COOL:
      return climate::CLIMATE_MODE_COOL;
    case KELON168_MODE_DRY:
      return climate::CLIMATE_MODE_DRY;
    case KELON168_MODE_FAN:
      return climate::CLIMATE_MODE_FAN_ONLY;
    case KELON168_MODE_AUTO:
    default:
      return climate::CLIMATE_MODE_HEAT_COOL;
  }
}

void HisenseKelonIRClimate::set_fan_(Kelon168Data *data, climate::ClimateFanMode fan_mode) const {
  data->state[2] &= ~0x03;
  data->state[17] &= ~0x40;
  switch (fan_mode) {
    case climate::CLIMATE_FAN_LOW:
      data->state[2] |= 0x03;
      data->state[17] |= 0x40;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      data->state[2] |= 0x02;
      break;
    case climate::CLIMATE_FAN_HIGH:
      data->state[2] |= 0x01;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      break;
  }
}

climate::ClimateFanMode HisenseKelonIRClimate::decode_fan_(const Kelon168Data &data) const {
  const uint8_t fan = data.state[2] & 0x03;
  if (fan == 0x03)
    return climate::CLIMATE_FAN_LOW;
  if (fan == 0x02)
    return climate::CLIMATE_FAN_MEDIUM;
  if (fan == 0x01)
    return climate::CLIMATE_FAN_HIGH;
  return climate::CLIMATE_FAN_AUTO;
}

void HisenseKelonIRClimate::log_changed_bytes_(const Kelon168Data &data) const {
  if (!this->have_last_tx_) {
    ESP_LOGV(TAG, "No previous Kelon168 state available for changed-byte mask");
    return;
  }
  char buffer[KELON168_STATE_LENGTH * 3 + 1];
  size_t pos = 0;
  bool any = false;
  for (uint8_t i = 0; i < KELON168_STATE_LENGTH; i++) {
    if (this->last_tx_.state[i] == data.state[i])
      continue;
    any = true;
    pos += snprintf(buffer + pos, sizeof(buffer) - pos, "%u ", i);
  }
  ESP_LOGV(TAG, "Changed Kelon168 byte indexes: %s", any ? buffer : "none");
}

}  // namespace hisense_kelon_ir
}  // namespace esphome
