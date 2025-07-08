#include "protocol_decoder.h"
#include "hisense_ac.h"
#include "common_types.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace hisense_ac {

static const char *const TAG = "hisense_ac.decoder";

ProtocolDecoder::ValidationResult ProtocolDecoder::validate_message(const std::vector<uint8_t>& data) {
  ValidationResult result = {false, 0, 0};
  
  // Check minimum length
  if (data.size() < 16) {
    ESP_LOGD(TAG, "Message too short: %zu bytes", data.size());
    return result;
  }
  
  // Check header
  if (data[0] != MessageTypes::HEADER_1 || data[1] != MessageTypes::HEADER_2) {
    ESP_LOGW(TAG, "Invalid header: %02X %02X (expected: %02X %02X) - possible UART corruption", 
             data[0], data[1], MessageTypes::HEADER_1, MessageTypes::HEADER_2);
    return result;
  }
  
  // Check footer
  if (data[data.size()-2] != MessageTypes::FOOTER_1 || data[data.size()-1] != MessageTypes::FOOTER_2) {
    ESP_LOGD(TAG, "Invalid footer: %02X %02X", data[data.size()-2], data[data.size()-1]);
    return result;
  }
  
  // Extract packet structure information (following manual.py layout)
  uint8_t padding_byte_1 = data[3];    // Should be PADDING_BYTE_1
  uint8_t packet_length = data[4];     // Length field
  result.msg_packet_type = data[13];   // Message packet type
  result.msg_sub_type = data[14];      // Message sub-type
  
  // Verify padding byte
  if (padding_byte_1 != MessageTypes::PADDING_BYTE_1) {
    ESP_LOGD(TAG, "Invalid padding byte 1: %02X (expected %02X)", padding_byte_1, MessageTypes::PADDING_BYTE_1);
    return result;
  }
  
  // Check length field
  size_t expected_length = packet_length + 9;  // Length field + static overhead
  if (data.size() != expected_length) {
    ESP_LOGD(TAG, "Length mismatch: declared=%d, actual=%zu, expected=%zu", 
             packet_length, data.size(), expected_length);
    return result;
  }
  
  // CRC validation (following manual.py step_2_check_crc)
  std::vector<uint8_t> crc_data(data.begin() + 2, data.end() - 4); // From byte 2 to -4
  std::vector<uint8_t> crc_msg(data.end() - 4, data.end() - 2);    // CRC bytes
  
  uint16_t crc_received = (crc_msg[0] << 8) | crc_msg[1];
  uint16_t crc_calculated = 0;
  for (uint8_t byte : crc_data) {
    crc_calculated += byte;
  }
  
  if (crc_received != crc_calculated) {
    ESP_LOGD(TAG, "CRC mismatch: received=0x%04X, calculated=0x%04X", crc_received, crc_calculated);
    return result;
  }
  
  // All checks passed
  result.valid = true;
  ESP_LOGD(TAG, "Valid message: type=0x%02X, sub_type=0x%02X", 
           result.msg_packet_type, result.msg_sub_type);
  
  return result;
}

uint16_t ProtocolDecoder::calculate_crc(const std::vector<uint8_t>& data, size_t start, size_t end) {
  uint16_t crc = 0;
  for (size_t i = start; i < end; i++) {
    crc += data[i];
  }
  return crc;
}

uint32_t ProtocolDecoder::extract_bits(const std::vector<uint8_t>& data, size_t offset, size_t size) {
  // Extract bits from payload data exactly like manual.py
  // manual.py converts entire payload to one big binary string, then slices it
  size_t payload_start = 16;
  size_t payload_end = data.size() - 4;
  
  // Safety checks
  if (data.size() < payload_start + 4) {
    ESP_LOGD(TAG, "Message too short for payload extraction: %zu bytes", data.size());
    return 0;
  }
  
  if (payload_end <= payload_start) {
    ESP_LOGD(TAG, "Invalid payload bounds: start=%zu, end=%zu", payload_start, payload_end);
    return 0;
  }
  
  // Create payload vector 
  std::vector<uint8_t> payload(data.begin() + payload_start, data.begin() + payload_end);
  
  // Convert payload to binary string like manual.py does
  // Each byte contributes 8 bits to the string, MSB first
  std::string binary_string;
  binary_string.reserve(payload.size() * 8);
  
  for (size_t i = 0; i < payload.size(); i++) {
    uint8_t byte_val = payload[i];
    for (int bit = 7; bit >= 0; bit--) {
      binary_string += ((byte_val >> bit) & 1) ? '1' : '0';
    }
  }
  
  // Extract the requested bit slice
  if (offset + size > binary_string.length()) {
    ESP_LOGD(TAG, "Bit extraction out of range: offset=%zu, size=%zu, total_bits=%zu", 
             offset, size, binary_string.length());
    return 0;
  }
  
  std::string slice = binary_string.substr(offset, size);
  
  // Convert binary string to integer
  uint32_t result = 0;
  for (char bit : slice) {
    result = (result << 1) + (bit - '0');
  }
  
  return result;
}

void ProtocolDecoder::decode_status_message(const std::vector<uint8_t>& data) {
  ESP_LOGD(TAG, "Decoding status message (%zu bytes)", data.size());
  
  // Full status message - decode all parameters
  ESP_LOGD(TAG, "Decoding full status message");
  this->parent_->fan_status_ = this->extract_bits(data, FAN_STATUS.offset, FAN_STATUS.size);
  this->parent_->sleep_status_ = static_cast<SleepMode>(this->extract_bits(data, SLEEP_STATUS.offset, SLEEP_STATUS.size));
  this->parent_->mode_status_ = this->extract_bits(data, MODE_STATUS.offset, MODE_STATUS.size);
  this->parent_->run_status_ = this->extract_bits(data, ON_OFF_STATUS.offset, ON_OFF_STATUS.size) != 0;
  this->parent_->direction_status_ = this->extract_bits(data, DIRECTION.offset, DIRECTION.size);
  this->parent_->target_temp_ = this->extract_bits(data, SET_TEMPERATURE.offset, SET_TEMPERATURE.size);
  this->parent_->current_temp_ = this->extract_bits(data, INDOOR_TEMPERATURE_STATUS.offset, INDOOR_TEMPERATURE_STATUS.size);
  this->parent_->indoor_pipe_temperature_ = this->extract_bits(data, INDOOR_PIPE_TEMPERATURE.offset, INDOOR_PIPE_TEMPERATURE.size);
  this->parent_->indoor_humidity_setting_ = this->extract_bits(data, INDOOR_HUMIDITY_SETTING.offset, INDOOR_HUMIDITY_SETTING.size);
  this->parent_->indoor_humidity_status_ = this->extract_bits(data, INDOOR_HUMIDITY_STATUS.offset, INDOOR_HUMIDITY_STATUS.size);
  this->parent_->temperature_compensation_ = this->extract_bits(data, TEMPERATURE_COMPENSATION.offset, TEMPERATURE_COMPENSATION.size);
  this->parent_->poweron_hour_ = this->extract_bits(data, ON_TIMER_HOUR.offset, ON_TIMER_HOUR.size);
  this->parent_->poweron_minute_ = this->extract_bits(data, ON_TIMER_MINUTE.offset, ON_TIMER_MINUTE.size);
  this->parent_->poweron_status_ = this->extract_bits(data, ON_TIMER_STATUS.offset, ON_TIMER_STATUS.size) != 0;
  this->parent_->poweroff_hour_ = this->extract_bits(data, OFF_TIMER_HOUR.offset, OFF_TIMER_HOUR.size);
  this->parent_->poweroff_minute_ = this->extract_bits(data, OFF_TIMER_MINUTE.offset, OFF_TIMER_MINUTE.size);
  this->parent_->poweroff_status_ = this->extract_bits(data, OFF_TIMER_STATUS.offset, OFF_TIMER_STATUS.size) != 0;
  this->parent_->up_down_ = this->extract_bits(data, SWING_UP_DOWN.offset, SWING_UP_DOWN.size) != 0;
  this->parent_->left_right_ = this->extract_bits(data, SWING_LEFT_RIGHT.offset, SWING_LEFT_RIGHT.size) != 0;
  this->parent_->eco_mode_ = this->extract_bits(data, ECO_MODE.offset, ECO_MODE.size) != 0;
  this->parent_->boost_mode_ = this->extract_bits(data, BOOST_MODE.offset, BOOST_MODE.size) != 0;
  this->parent_->quiet_mode_ = this->extract_bits(data, QUIET_MODE.offset, QUIET_MODE.size) != 0;
  this->parent_->display_on_ = this->extract_bits(data, DISPLAY_STATUS.offset, DISPLAY_STATUS.size) != 0;
  this->parent_->command_received_ = this->extract_bits(data, COMMAND_RECEIVED.offset, COMMAND_RECEIVED.size);
  this->parent_->indoor_eeprom_ = this->extract_bits(data, INDOOR_EEPROM.offset, INDOOR_EEPROM.size) != 0;
  this->parent_->compressor_frequency_setting_ = this->extract_bits(data, COMPRESSOR_FREQUENCY.offset, COMPRESSOR_FREQUENCY.size);
  this->parent_->outdoor_temperature_ = this->extract_bits(data, OUTDOOR_TEMPERATURE.offset, OUTDOOR_TEMPERATURE.size);
  this->parent_->outdoor_condenser_temperature_ = this->extract_bits(data, OUTDOOR_CONDENSER_TEMPERATURE.offset, OUTDOOR_CONDENSER_TEMPERATURE.size);
  this->parent_->compressor_exhaust_temperature_ = this->extract_bits(data, COMPRESSOR_EXHAUST_TEMPERATURE.offset, COMPRESSOR_EXHAUST_TEMPERATURE.size);
  this->parent_->IAB_ = this->extract_bits(data, IAB.offset, IAB.size);
  this->parent_->IBC_ = this->extract_bits(data, IBC.offset, IBC.size);
  this->parent_->IUV_ = this->extract_bits(data, IUV.offset, IUV.size);

  //Legacy implementation, to be cleaned up later
  this->decode_modes_and_power();
  this->decode_fan_and_swing();
  this->decode_sleep_mode();
  
  // Update ESPHome climate state
  this->parent_->update_climate_state();
}

void ProtocolDecoder::decode_modes_and_power() {
  // Update internal state
  this->parent_->power_state_ = this->parent_->run_status_;
  
  // Map AC mode to climate mode
  if (!this->parent_->run_status_) {
    this->parent_->current_mode_ = climate::CLIMATE_MODE_OFF;
  } else {
    switch (this->parent_->mode_status_) {
      case 0: this->parent_->current_mode_ = climate::CLIMATE_MODE_FAN_ONLY; break;
      case 1: this->parent_->current_mode_ = climate::CLIMATE_MODE_HEAT; break;
      case 2: this->parent_->current_mode_ = climate::CLIMATE_MODE_COOL; break;
      case 3: this->parent_->current_mode_ = climate::CLIMATE_MODE_DRY; break;
      case 4:  //AC reports this mode if in auto mode but temp has been adjusted via the remote
      case 5:  //AC reports this mode if in auto mode but temp has been adjusted via the remote
      case 6:  //AC reports this mode if in auto mode but temp has been adjusted via the remote
      case 7: this->parent_->current_mode_ = climate::CLIMATE_MODE_AUTO; break; // Auto mode
      default: this->parent_->current_mode_ = climate::CLIMATE_MODE_OFF; break;
    }
  }
  
  ESP_LOGD(TAG, "Power: %s, Mode: %d, fan: %d, Sleep: %d", 
           this->parent_->run_status_ ? "ON" : "OFF", this->parent_->mode_status_, 
           this->parent_->fan_status_, this->parent_->sleep_status_);
}

void ProtocolDecoder::decode_fan_and_swing() {
  // Map fan levels to ESPHome modes
  this->parent_->actual_fan_level_ = this->parent_->fan_status_;
  this->parent_->current_custom_fan_mode_.clear();
  this->parent_->custom_fan_mode = std::string();
  
  switch (this->parent_->fan_status_) {
    case 0: this->parent_->current_fan_mode_ = climate::CLIMATE_FAN_OFF; break;
    case 1: this->parent_->current_fan_mode_ = climate::CLIMATE_FAN_AUTO; break;
    case 2: 
      this->parent_->current_fan_mode_ = climate::CLIMATE_FAN_ON;
      this->parent_->current_custom_fan_mode_ = "Quiet";
      this->parent_->custom_fan_mode = std::string("Quiet");
      break;
    case 10: this->parent_->current_fan_mode_ = climate::CLIMATE_FAN_LOW; break;
    case 12: 
      this->parent_->current_fan_mode_ = climate::CLIMATE_FAN_ON;
      this->parent_->current_custom_fan_mode_ = "Med-Low";
      this->parent_->custom_fan_mode = std::string("Med-Low");
      break;
    case 14: this->parent_->current_fan_mode_ = climate::CLIMATE_FAN_MEDIUM; break;
    case 16: 
      this->parent_->current_fan_mode_ = climate::CLIMATE_FAN_ON;
      this->parent_->current_custom_fan_mode_ = "Med-High";
      this->parent_->custom_fan_mode = std::string("Med-High");
      break;
    case 18: this->parent_->current_fan_mode_ = climate::CLIMATE_FAN_HIGH; break;
    default: this->parent_->current_fan_mode_ = climate::CLIMATE_FAN_AUTO; break;
  }
  
  // Decode swing modes: direction_status enables up_down, left_right is independent
  bool vertical_active = (this->parent_->direction_status_ == 2) && this->parent_->up_down_;
  bool horizontal_active = this->parent_->left_right_;
  
  if (vertical_active && horizontal_active) {
    this->parent_->current_swing_mode_ = climate::CLIMATE_SWING_BOTH;
  } else if (vertical_active) {
    this->parent_->current_swing_mode_ = climate::CLIMATE_SWING_VERTICAL;
  } else if (horizontal_active) {
    this->parent_->current_swing_mode_ = climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    this->parent_->current_swing_mode_ = climate::CLIMATE_SWING_OFF;
  }
}

void ProtocolDecoder::decode_sleep_mode() {
  // Add null pointer check
  if (this->parent_ == nullptr) {
    ESP_LOGW("decoder", "Parent is null in decode_sleep_mode");
    return;
  }
  
  // Add bounds checking for enum
  if (this->parent_->sleep_status_ < SleepMode::SLEEP_OFF || 
      this->parent_->sleep_status_ > SleepMode::SLEEP_KIDS) {
    ESP_LOGW("decoder", "Invalid sleep status: %d", this->parent_->sleep_status_);
    this->parent_->sleep_status_ = SleepMode::SLEEP_OFF;  // Reset to safe default
    return;
  }
  
  if (this->parent_->sleep_status_ == SleepMode::SLEEP_OFF) {
    this->parent_->set_sleep_enabled(false);
  } else {
    this->parent_->set_sleep_enabled(true);
    this->parent_->set_preferred_sleep_mode(static_cast<SleepMode>(this->parent_->sleep_status_));
  }
}

}  // namespace hisense_ac
}  // namespace esphome