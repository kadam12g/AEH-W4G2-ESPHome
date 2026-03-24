#include "command_sender.h"
#include "hisense_ac.h"
#include "common_types.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace hisense_ac {

static const char *const TAG = "hisense_ac.sender";

// Protocol initialization messages
static const uint8_t INIT_MESSAGE_1[] = {
  0xF4, 0xF5, 0x00, 0x40, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x01, 0x00, 0x00, 0x0A, 0x04, 0x00, 0x01, 0x58, 0xF4, 0xFB
};

static const uint8_t INIT_MESSAGE_2[] = {
  0xF4, 0xF5, 0x00, 0x40, 0x0B, 0x00, 0x00, 0x01, 0x01, 0xFE, 0x01, 0x00, 0x00, 0x07, 0x01, 0x00, 0x01, 0x54, 0xF4, 0xFB
};

static const uint8_t INIT_MESSAGE_3[] = {
  0xF4, 0xF5, 0x00, 0x40, 0x0B, 0x00, 0x00, 0x01, 0x01, 0xFE, 0x01, 0x00, 0x00, 0x66, 0x40, 0x00, 0x01, 0xF2, 0xF4, 0xFB
};

static const uint8_t WIFI_STATUS_MESSAGE[] = {
  0xF4, 0xF5, 0x00, 0x40, 0x13, 0x00, 0x00, 0x01, 0x01, 0xFE, 0x01, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x80, 0x80, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x74, 0xF4, 0xFB
};

static const uint8_t STATUS_REQUEST_MESSAGE[] = {
  0xF4, 0xF5, 0x00, 0x40, 0x0C, 0x00, 0x00, 0x01, 0x01, 0xFE, 0x01, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x01, 0xB3, 0xF4, 0xFB
};

// ===== BIT MANIPULATION HELPERS =====

void CommandSender::set_bit(uint16_t bit_position, bool value) {
  if (bit_position >= 240) return;  // 30 bytes * 8 bits = 240 bits max
  
  // Bit positions are sequential in the 30-byte payload, but with MSB-first bit ordering
  uint8_t byte_index = bit_position / 8;
  uint8_t bit_index = 7 - (bit_position % 8);  // Reverse bit order for correct endianness
  
  if (value) {
    this->command_data_[byte_index] |= (1 << bit_index);
  } else {
    this->command_data_[byte_index] &= ~(1 << bit_index);
  }
  
  ESP_LOGD(TAG, "Set bit %d to %d (byte %d, bit %d) -> 0x%02X", 
           bit_position, value, byte_index, bit_index, this->command_data_[byte_index]);
}

// Sets ghe given command, by shifting the raw value left by 1 and adding the control bit
void CommandSender::set_command_bits(uint16_t base_offset, uint8_t size_bits, uint32_t raw_value) {
  if (base_offset + size_bits > 240) return;  // Safety check
  
  // Clear existing bits in the range
  for (int i = base_offset; i < base_offset + size_bits; i++) {
    this->set_bit(i, false);
  }
  
  // Create the command: raw_value shifted left by 1 + control bit
  uint32_t command = (raw_value << 1) | 0x01;
  
  // Set the bits (MSB first - reverse bit order within the field)
  for (int i = 0; i < size_bits; i++) {
    bool bit_value = (command >> i) & 1;
    this->set_bit(base_offset + (size_bits - 1 - i), bit_value);
  }
  
  ESP_LOGD(TAG, "Command bits set with offset=%d, size=%d, raw_value=%d -> 0x%X", 
           base_offset, size_bits, raw_value, command);
}

// ===== COMMAND BUFFER MANAGEMENT =====

void CommandSender::initialize_command_buffer() {
  ESP_LOGD(TAG, "Initializing command buffer with base template");
  command_data_.resize(30, 0);  // Resize to 30 bytes (240 bits)

  // Clear the command buffer first (30 bytes = 240 bits)
  this->clear_command_buffer();
  
  ESP_LOGD(TAG, "Command buffer initialized, ready for chainable setters");
}

void CommandSender::clear_command_buffer() {
  ESP_LOGD(TAG, "Clearing command buffer");
  std::fill(this->command_data_.begin(), this->command_data_.end(), 0);
}

void CommandSender::send_prepared_command() {
  ESP_LOGD(TAG, "Sending prepared command from buffer");

    // Set bit 61 to make the AC buzz when it receives the command
    if (this->parent_->beep_enabled_) {
      this->set_beep_enabled(true);
    }
  
  // Build complete message following the reference implementation structure
  std::vector<uint8_t> message;
  
  // Header
  message.push_back(0xF4);
  message.push_back(0xF5);
  
  // Request type
  message.push_back(0x00);  // 0x00 = request
  
  // Padding byte 1
  message.push_back(0x40);
  
  // Packet length (30 bytes data + 11 header bytes = 41 = 0x29)
  message.push_back(0x29);
  
  // Padding bytes 2 (8 bytes)
  message.push_back(0x00);
  message.push_back(0x00);
  message.push_back(0x01);
  message.push_back(0x01);
  message.push_back(0xFE);
  message.push_back(0x01);
  message.push_back(0x00);
  message.push_back(0x00);
  
  // Packet type and sub-type (101_0 means packet type 101, sub-type 0)
  message.push_back(0x65);  // 101 in decimal = 0x65
  message.push_back(0x00);  // sub-type 0
  
  // Padding byte 3 (for request)
  message.push_back(0x00);
  
  // Add the 30-byte payload from the command buffer
  message.insert(message.end(), this->command_data_.begin(), this->command_data_.end());
  
  // Calculate CRC (from byte 2 to end of data, excluding the final CRC and footer)
  uint16_t crc = this->calculate_crc(message, 2, message.size());
  message.push_back((crc >> 8) & 0xFF);
  message.push_back(crc & 0xFF);
  
  // Footer
  message.push_back(0xF4);
  message.push_back(0xFB);
  
  ESP_LOGD(TAG, "Sending prepared command with %zu total bytes (should be 50)", message.size());
  this->send_message(message);
  this->parent_->command_state_.pending = true;
  this->parent_->command_state_.last_send_time = millis();
}

// ===== CHAINABLE PROPERTY SETTERS =====

void CommandSender::set_fan_mode(CommandSender::CommandFanMode mode) {
  this->set_command_bits(CommandSender::FAN_STATUS.offset, CommandSender::FAN_STATUS.size, static_cast<uint8_t>(mode));
}

void CommandSender::set_sleep_mode(SleepMode mode) {
  // Convert to command value (divide by 2) and set the bits
  uint8_t command_value = SleepModeUtils::to_command_value(mode);
  this->set_command_bits(CommandSender::SLEEP_STATUS.offset, CommandSender::SLEEP_STATUS.size, command_value);
}

void CommandSender::set_mode(CommandSender::CommandClimateMode mode) {
  this->set_command_bits(CommandSender::MODE_STATUS.offset, CommandSender::MODE_STATUS.size, static_cast<uint8_t>(mode));
}

void CommandSender::set_power(bool power_on) {
  this->set_command_bits(CommandSender::ON_OFF_STATUS.offset, CommandSender::ON_OFF_STATUS.size, static_cast<uint8_t>(power_on));
}

void CommandSender::set_temperature(int temperature) {
  int temp = std::max(16, std::min(30, temperature));  // Clamp to valid range
  
  this->set_command_bits(CommandSender::SET_TEMPERATURE.offset, CommandSender::SET_TEMPERATURE.size, temp);
}

void CommandSender::set_beep_enabled(bool enabled) {
  this->set_bit(CommandSender::BEEP.offset, enabled);
}

void CommandSender::set_swing_up_down(bool enabled) {
  this->set_command_bits(CommandSender::SWING_UP_DOWN.offset, CommandSender::SWING_UP_DOWN.size, static_cast<uint8_t>(enabled));
}

void CommandSender::set_swing_left_right(bool enabled) {
  this->set_command_bits(CommandSender::SWING_LEFT_RIGHT.offset, CommandSender::SWING_LEFT_RIGHT.size, static_cast<uint8_t>(enabled));
}

void CommandSender::set_eco_mode(bool enabled) {
  this->set_command_bits(CommandSender::ECO_MODE.offset, CommandSender::ECO_MODE.size, static_cast<uint8_t>(enabled));
}

void CommandSender::set_boost_mode(bool enabled) {
  this->set_command_bits(CommandSender::BOOST_MODE.offset, CommandSender::BOOST_MODE.size, static_cast<uint8_t>(enabled));
}

void CommandSender::set_quiet_mode(bool enabled) {
  this->set_command_bits(CommandSender::QUIET_MODE.offset, CommandSender::QUIET_MODE.size, static_cast<uint8_t>(enabled));
}

void CommandSender::set_display_status(bool enabled) {
  this->set_command_bits(CommandSender::DISPLAY_STATUS.offset, CommandSender::DISPLAY_STATUS.size, static_cast<uint8_t>(enabled));
}

void CommandSender::send_init_message_1() {
  std::vector<uint8_t> message(INIT_MESSAGE_1, INIT_MESSAGE_1 + sizeof(INIT_MESSAGE_1));
  this->send_message(message);
}

void CommandSender::send_init_message_2() {
  std::vector<uint8_t> message(INIT_MESSAGE_2, INIT_MESSAGE_2 + sizeof(INIT_MESSAGE_2));
  this->send_message(message);
}

void CommandSender::send_init_message_3() {
  std::vector<uint8_t> message(INIT_MESSAGE_3, INIT_MESSAGE_3 + sizeof(INIT_MESSAGE_3));
  this->send_message(message);
}

void CommandSender::send_wifi_status() {
  std::vector<uint8_t> message(WIFI_STATUS_MESSAGE, WIFI_STATUS_MESSAGE + sizeof(WIFI_STATUS_MESSAGE));
  this->send_message(message);
}

void CommandSender::send_status_request() {
  std::vector<uint8_t> message(STATUS_REQUEST_MESSAGE, STATUS_REQUEST_MESSAGE + sizeof(STATUS_REQUEST_MESSAGE));
  this->send_message(message);
}

CommandSender::CommandClimateMode CommandSender::convert_mode(climate::ClimateMode esphome_mode) {
  switch (esphome_mode) {
    case climate::CLIMATE_MODE_COOL: return CommandClimateMode::COOL;
    case climate::CLIMATE_MODE_HEAT: return CommandClimateMode::HEAT;
    case climate::CLIMATE_MODE_DRY: return CommandClimateMode::DRY;
    case climate::CLIMATE_MODE_AUTO: return CommandClimateMode::AUTO;
    case climate::CLIMATE_MODE_FAN_ONLY: return CommandClimateMode::FAN_ONLY;
    default: 
      ESP_LOGW(TAG, "Unsupported climate mode: %d", static_cast<int>(esphome_mode));
      return CommandClimateMode::AUTO;
  }
}

CommandSender::CommandFanMode CommandSender::convert_fan_mode(climate::ClimateFanMode esphome_fan) {
  switch (esphome_fan) {
    case climate::CLIMATE_FAN_AUTO: return CommandFanMode::AUTO;
    case climate::CLIMATE_FAN_LOW: return CommandFanMode::FAN_LOW;
    case climate::CLIMATE_FAN_MEDIUM: return CommandFanMode::FAN_MEDIUM;
    case climate::CLIMATE_FAN_HIGH: return CommandFanMode::FAN_HIGH;
    default: 
      ESP_LOGW(TAG, "Unsupported fan mode: %d", static_cast<int>(esphome_fan));
      return CommandFanMode::AUTO;
  }
}

CommandSender::CommandFanMode CommandSender::convert_fan_mode(const std::string& custom_fan_mode) {
  if (custom_fan_mode == "Med-Low") {
    return CommandFanMode::FAN_LOW_MEDIUM;
  } else if (custom_fan_mode == "Med-High") {
    return CommandFanMode::FAN_MEDIUM_HIGH;
  } else {
    ESP_LOGW(TAG, "Unsupported custom fan mode: %s", custom_fan_mode.c_str());
    return CommandFanMode::AUTO; // Default to AUTO
  }
}

void CommandSender::set_mode(climate::ClimateMode esphome_mode) {
  this->set_mode(convert_mode(esphome_mode));
}

void CommandSender::set_fan_mode(climate::ClimateFanMode esphome_fan) {
  this->set_fan_mode(convert_fan_mode(esphome_fan));
}

void CommandSender::set_fan_mode(const std::string& custom_fan_mode) {
  this->set_fan_mode(convert_fan_mode(custom_fan_mode));
}

void CommandSender::set_swing_mode(climate::ClimateSwingMode esphome_swing) {
  switch (esphome_swing) {
    case climate::CLIMATE_SWING_OFF:
      this->set_swing_up_down(false);
      this->set_swing_left_right(false);
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      this->set_swing_up_down(true);
      this->set_swing_left_right(false);
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      this->set_swing_up_down(false);
      this->set_swing_left_right(true);
      break;
    case climate::CLIMATE_SWING_BOTH:
      this->set_swing_up_down(true);
      this->set_swing_left_right(true);
      break;
    default:
      ESP_LOGW(TAG, "Unsupported swing mode: %d", static_cast<int>(esphome_swing));
  }
}

void CommandSender::send_message(const std::vector<uint8_t>& message) {
  this->log_hex_message("AC_TX", message.data(), message.size());
  if (this->parent_->flow_control_pin_ != nullptr) {
    this->parent_->flow_control_pin_->digital_write(true);
  }
  // Send through parent's UARTDevice interface
  this->parent_->write_array(message.data(), message.size());

  if (this->parent_->flow_control_pin_ != nullptr) {
    this->parent_->flow_control_pin_->digital_write(false);
  }

  this->parent_->messages_sent_++;
}

uint16_t CommandSender::calculate_crc(const std::vector<uint8_t>& data, size_t start, size_t end) {
  uint16_t crc = 0;
  for (size_t i = start; i < end; i++) {
    crc += data[i];
  }
  return crc;
}

void CommandSender::log_hex_message(const char* prefix, const uint8_t* data, size_t len) {
  std::string hex_str;
  hex_str.reserve(len * 3);
  
  for (size_t i = 0; i < len; i++) {
    if (i > 0) hex_str += " ";
    hex_str += str_sprintf("%02X", data[i]);
  }
  
  ESP_LOGD(TAG, "%s (%zu bytes): %s", prefix, len, hex_str.c_str());
}


}  // namespace hisense_ac
}  // namespace esphome