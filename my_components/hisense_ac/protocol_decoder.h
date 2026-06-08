#pragma once

#include <vector>
#include <cstdint>
#include "common_types.h"

namespace esphome {
namespace hisense_ac {

// Forward declaration
class HisenseAC;

class ProtocolDecoder {
 public:
  explicit ProtocolDecoder(HisenseAC* parent) : parent_(parent) {}

  enum class FanMode : uint8_t {
    FAN_OFF = 0,
    FAN_AUTO = 1,
    FAN_QUIET = 2,
    FAN_LOW = 10, 
    FAN_LOW_MEDIUM = 12, 
    FAN_MEDIUM = 14, 
    FAN_MEDIUM_HIGH = 16, 
    FAN_HIGH = 18 
  };

  enum class ClimateMode : uint8_t {
    FAN_ONLY = 0,
    HEAT = 1,
    COOL = 2,
    DRY = 3,
    AUTO = 7
  };

  struct StatusField {
    uint16_t offset;
    uint8_t size;
  };

  static constexpr StatusField FAN_STATUS = {0, 8};
  static constexpr StatusField SLEEP_STATUS = {8, 8};
  static constexpr StatusField MODE_STATUS = {16, 4};
  static constexpr StatusField ON_OFF_STATUS = {20, 1};
  static constexpr StatusField DIRECTION = {22, 2};
  static constexpr StatusField SET_TEMPERATURE = {24, 8};
  static constexpr StatusField INDOOR_TEMPERATURE_STATUS = {32, 8};
  static constexpr StatusField INDOOR_PIPE_TEMPERATURE = {40, 8};
  static constexpr StatusField INDOOR_HUMIDITY_SETTING = {48, 8};
  static constexpr StatusField INDOOR_HUMIDITY_STATUS = {56, 8};
  static constexpr StatusField TEMPERATURE_COMPENSATION = {80, 5};
  static constexpr StatusField ON_TIMER_HOUR = {112, 5};
  static constexpr StatusField ON_TIMER_MINUTE = {120, 6};
  static constexpr StatusField ON_TIMER_STATUS = {127, 1};
  static constexpr StatusField OFF_TIMER_HOUR = {128, 5};
  static constexpr StatusField OFF_TIMER_MINUTE = {136, 6};
  static constexpr StatusField OFF_TIMER_STATUS = {143, 1};
  static constexpr StatusField SWING_UP_DOWN = {152, 1};
  static constexpr StatusField SWING_LEFT_RIGHT = {153, 1};
  static constexpr StatusField ECO_MODE = {157, 1};
  static constexpr StatusField BOOST_MODE = {158, 1};
  static constexpr StatusField QUIET_MODE = {165, 1};
  static constexpr StatusField DISPLAY_STATUS = {168, 1};
  static constexpr StatusField COMMAND_RECEIVED = {178, 4};
  static constexpr StatusField INDOOR_EEPROM = {183, 1};
  static constexpr StatusField COMPRESSOR_FREQUENCY = {208, 8};
  static constexpr StatusField OUTDOOR_TEMPERATURE = {224, 8};
  static constexpr StatusField OUTDOOR_CONDENSER_TEMPERATURE = {232, 8};
  static constexpr StatusField COMPRESSOR_EXHAUST_TEMPERATURE = {240, 8};
  static constexpr StatusField IAB = {312, 8};
  static constexpr StatusField IBC = {320, 8};
  static constexpr StatusField IUV = {352, 8};

  // Protocol message types and constants
  struct MessageTypes {
    // Packet types (data[2])
    static constexpr uint8_t PACKET_REQUEST = 0x00;
    static constexpr uint8_t PACKET_RESPONSE = 0x01;
    
    // Protocol constants
    static constexpr uint8_t HEADER_1 = 0xF4;
    static constexpr uint8_t HEADER_2 = 0xF5;
    static constexpr uint8_t FOOTER_1 = 0xF4;
    static constexpr uint8_t FOOTER_2 = 0xFB;
    static constexpr uint8_t PADDING_BYTE_1 = 0x40;
    
    // Message packet types (data[13]) and sub-types (data[14])
    struct Init1 {
      static constexpr uint8_t TYPE = 0x0A;
      static constexpr uint8_t SUB_TYPE = 0x04;
    };
    
    struct Init2 {
      static constexpr uint8_t TYPE = 0x07;
      static constexpr uint8_t SUB_TYPE = 0x01;
    };
    
    struct Init3 {
      static constexpr uint8_t TYPE = 0x66;
      static constexpr uint8_t SUB_TYPE = 0x40;
    };
    
    struct WiFiStatus {
      static constexpr uint8_t TYPE = 0x1E;
      static constexpr uint8_t SUB_TYPE = 0x00;
    };
    
    struct StatusRequest {
      static constexpr uint8_t TYPE = 0x66;
      static constexpr uint8_t SUB_TYPE = 0x00;
    };
    
    struct Command {
      static constexpr uint8_t TYPE = 0x65;
      static constexpr uint8_t SUB_TYPE = 0x00;
    };
  };
  
  // Message validation result
  struct ValidationResult {
    bool valid;
    uint8_t msg_packet_type; // e.g., 0x66 = 102, 0x65 = 101, 0x1E = 30
    uint8_t msg_sub_type;    // sub-type (usually 0x00)
  };
  
  // Message validation and decoding
  ValidationResult validate_message(const std::vector<uint8_t>& data);
  void decode_status_message(const std::vector<uint8_t>& data);
  
  // Bit extraction utility (matches manual.py approach)
  uint32_t extract_bits(const std::vector<uint8_t>& data, size_t offset, size_t size);
  
  // CRC calculation
  uint16_t calculate_crc(const std::vector<uint8_t>& data, size_t start, size_t end);
  
 private:
  HisenseAC* parent_;
  
  // Status decoding helpers
  void decode_modes_and_power();
  void decode_fan_and_swing();
  void decode_sleep_mode();
};

}  // namespace hisense_ac
}  // namespace esphome