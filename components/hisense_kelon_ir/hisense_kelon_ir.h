#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "kelon168_protocol.h"

namespace esphome {
namespace hisense_kelon_ir {

enum EnsurePowerMode {
  ENSURE_POWER_SMART,
  ENSURE_POWER_SUPER,
  ENSURE_POWER_NONE,
};

class HisenseKelonIRClimate : public climate_ir::ClimateIR {
 public:
  HisenseKelonIRClimate()
      : climate_ir::ClimateIR(
            16.0f, 32.0f, 1.0f, true, true,
            {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
             climate::CLIMATE_FAN_HIGH},
            {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
            {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_SLEEP, climate::CLIMATE_PRESET_BOOST}) {}

  void setup() override;
  void dump_config() override;
  void set_ensure_power(EnsurePowerMode mode) { this->ensure_power_ = mode; }
  void set_ensure_power_on_boot(bool ensure_power_on_boot) { this->ensure_power_on_boot_ = ensure_power_on_boot; }

  void send_follow_me(float temperature, bool enabled);
  void send_display_off();

 protected:
  void control(const climate::ClimateCall &call) override;
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  Kelon168Data build_state_(climate::ClimateMode mode, float target_temperature,
                            climate::ClimateFanMode fan_mode, climate::ClimateSwingMode swing_mode,
                            climate::ClimatePreset preset, bool power_toggle, uint8_t command) const;
  void transmit_kelon_(Kelon168Data data);
  void ensure_power_on_();
  void apply_follow_me_(Kelon168Data *data) const;
  void apply_received_state_(const Kelon168Data &data);
  uint8_t encode_mode_(climate::ClimateMode mode) const;
  climate::ClimateMode decode_mode_(uint8_t mode) const;
  void set_fan_(Kelon168Data *data, climate::ClimateFanMode fan_mode) const;
  climate::ClimateFanMode decode_fan_(const Kelon168Data &data) const;
  void log_changed_bytes_(const Kelon168Data &data) const;

  EnsurePowerMode ensure_power_{ENSURE_POWER_SMART};
  Kelon168Data last_tx_{};
  bool have_last_tx_{false};
  bool power_known_{false};
  bool should_ensure_power_{false};
  bool ensure_power_on_boot_{false};
  bool follow_me_enabled_{false};
  uint8_t follow_me_temperature_{0};
};

using Kelon168Dumper = remote_base::RemoteReceiverDumper<Kelon168Protocol>;

template<typename... Ts> class FollowMeAction : public Action<Ts...> {
 public:
  explicit FollowMeAction(HisenseKelonIRClimate *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, temperature)
  TEMPLATABLE_VALUE(bool, enabled)

 protected:
  void play(const Ts &...x) override {
    this->parent_->send_follow_me(this->temperature_.value(x...), this->enabled_.value_or(x..., true));
  }

  HisenseKelonIRClimate *parent_;
};

template<typename... Ts> class DisplayOffAction : public Action<Ts...> {
 public:
  explicit DisplayOffAction(HisenseKelonIRClimate *parent) : parent_(parent) {}

 protected:
  void play(const Ts &...x) override { this->parent_->send_display_off(); }

  HisenseKelonIRClimate *parent_;
};

}  // namespace hisense_kelon_ir
}  // namespace esphome
