#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include "esphome/components/climate/climate.h"
#include "esphome/core/gpio.h"
#include "common_types.h"

namespace esphome {
namespace hisense_ac {

// Forward declaration
class HisenseAC;

class CommandSender {
 public:
  explicit CommandSender(HisenseAC* parent) : parent_(parent) {}

  // Note: Using command-specific fan mode values for bit field encoding
  enum class CommandFanMode : uint8_t { 
    AUTO = 0, 
    FAN_LOW = 5, 
    FAN_LOW_MEDIUM = 6, 
    FAN_MEDIUM = 7, 
    FAN_MEDIUM_HIGH = 8, 
    FAN_HIGH = 9 
  };
  
  // Note: Using command-specific climate mode values for bit field encoding
  enum class CommandClimateMode : uint8_t {
    FAN_ONLY = 0,
    HEAT = 1,
    COOL = 2,
    DRY = 3,
    AUTO = 4
  };

  struct CommandField {
    uint16_t offset;
    uint8_t size;
  };

  static constexpr CommandField FAN_STATUS = {0, 8};
  static constexpr CommandField SLEEP_STATUS = {8, 8};
  static constexpr CommandField MODE_STATUS = {16, 4};
  static constexpr CommandField ON_OFF_STATUS = {20, 2};
  static constexpr CommandField SET_TEMPERATURE = {24, 8};
  static constexpr CommandField BEEP = {61, 1};
  static constexpr CommandField SWING_UP_DOWN = {128, 2};
  static constexpr CommandField SWING_LEFT_RIGHT = {130, 2};
  static constexpr CommandField ECO_MODE = {138, 2};
  static constexpr CommandField BOOST_MODE = {140, 2};
  static constexpr CommandField QUIET_MODE = {154, 2};
  static constexpr CommandField DISPLAY_STATUS = {160, 2};

  void set_sleep_mode(SleepMode mode);  // Uses common SleepMode
  void set_mode(CommandClimateMode mode);
  void set_power(bool power_on);
  void set_temperature(int temperature);
  void set_beep_enabled(bool enabled);
  void set_swing_up_down(bool enabled);
  void set_swing_left_right(bool enabled);
  void set_eco_mode(bool enabled);
  void set_boost_mode(bool enabled);
  void set_quiet_mode(bool enabled);
  void set_display_status(bool enabled);

  //Conversion methods
  CommandClimateMode convert_mode(climate::ClimateMode esphome_mode);
  CommandFanMode convert_fan_mode(climate::ClimateFanMode esphome_fan_mode);
  CommandFanMode convert_fan_mode(const std::string& custom_fan_mode);
  SleepMode convert_swing_mode(climate::ClimateSwingMode esphome_swing_mode);

  void set_mode(climate::ClimateMode esphome_mode);
  
  void set_fan_mode(CommandFanMode mode);
  void set_fan_mode(climate::ClimateFanMode esphome_fan);
  void set_fan_mode(const std::string& custom_fan_mode);
  
  void set_swing_mode(climate::ClimateSwingMode esphome_swing);

  // Fixed message sending (for init/wifi status/status)
  void send_init_message_1();
  void send_init_message_2();
  void send_init_message_3();
  void send_wifi_status();
  void send_status_request();
  
  // Command buffer management
    // Command data buffer (30 bytes payload for type 0x65 and 0x66 commands)
  std::vector<uint8_t> command_data_;
  void initialize_command_buffer();
  void clear_command_buffer();
  void send_prepared_command();
  
  // Bit manipulation helper
  void set_bit(uint16_t bit_position, bool value);
  void set_command_bits(uint16_t base_offset, uint8_t size_bits, uint32_t value);
  
 private:
  HisenseAC* parent_;
  
  // Message utilities
  void send_message(const std::vector<uint8_t>& message);
  uint16_t calculate_crc(const std::vector<uint8_t>& data, size_t start, size_t end);
  void log_hex_message(const char* prefix, const uint8_t* data, size_t len);
};

}  // namespace hisense_ac
}  // namespace esphome