#include "hisense_ac.h"
#include "protocol_decoder.h"
#include "command_sender.h"
#include "common_types.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace hisense_ac {

static const char *const TAG = "hisense_ac";

HisenseAC::HisenseAC() : decoder_(nullptr), sender_(nullptr) {
  ESP_LOGI(TAG, "=== Hisense AC Constructor Called ===");
  ESP_LOGI(TAG, "Constructor - Component address: %p", this);
}

HisenseAC::~HisenseAC() {
  ESP_LOGI(TAG, "=== Hisense AC Destructor Called ===");
  if (decoder_) {
    delete decoder_;
    decoder_ = nullptr;
  }
  if (sender_) {
    delete sender_;
    sender_ = nullptr;
  }
}

void HisenseAC::setup() {
  ESP_LOGI(TAG, "=== Hisense AC Setup Starting ===");
  ESP_LOGI(TAG, "Setup - Component address: %p", this);
  ESP_LOGCONFIG(TAG, "Setting up Hisense AC...");
  
  // UART is automatically available through UARTDevice inheritance
  ESP_LOGI(TAG, "UART device initialized");

  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
  }
  ESP_LOGI(TAG, "Flow control pin setup complete");
  
  // Initialize helper components
  this->decoder_ = nullptr;
  this->sender_ = nullptr;
  
  ESP_LOGI(TAG, "Creating ProtocolDecoder...");
  this->decoder_ = new ProtocolDecoder(this);
  if (this->decoder_) {
    ESP_LOGI(TAG, "ProtocolDecoder created successfully at %p", this->decoder_);
  } else {
    ESP_LOGE(TAG, "ProtocolDecoder is null after creation");
    this->mark_failed();
    return;
  }
  
  ESP_LOGI(TAG, "Creating CommandSender...");
  this->sender_ = new CommandSender(this);
  if (this->sender_) {
    ESP_LOGI(TAG, "CommandSender created successfully at %p", this->sender_);
    // Initialize command data buffer (30 bytes)
    this->sender_->initialize_command_buffer();
  } else {
    ESP_LOGE(TAG, "CommandSender is null after creation");
    this->mark_failed();
    return;
  }
  
  // Set initial state - start with automatic initialization
  this->state_ = STATE_INIT_1;
  this->last_message_time_ = millis();
  this->last_state_time_ = millis();
  this->command_state_ = CommandState{};  // Initialize command state
  
  // Set climate initial state
  this->mode = climate::CLIMATE_MODE_OFF;
  this->target_temperature = 25;
  this->current_temperature = 25;
  this->fan_mode = climate::CLIMATE_FAN_AUTO;
  this->swing_mode = climate::CLIMATE_SWING_OFF;
  
  ESP_LOGI(TAG, "=== Hisense AC Setup Complete ===");
  ESP_LOGCONFIG(TAG, "Hisense AC setup complete");
}

float HisenseAC::get_setup_priority() const {
  // Set high priority to ensure UART is ready
  return esphome::setup_priority::DATA;
}

void HisenseAC::loop() {
  uint32_t now = millis();
  
  // Check if we should execute the next state action
  bool should_execute = false;
  
  if (this->automatic_execution_) {
    // Automatic mode: execute if enough time has passed
    should_execute = (now - this->last_state_time_) >= this->state_interval_;
  } else {
    // Manual mode: only execute when button is pressed
    should_execute = this->manual_step_requested_;
    if (this->manual_step_requested_) {
      this->manual_step_requested_ = false;  // Reset flag
      ESP_LOGI(TAG, "Manual step triggered");
    }
  }
  
  if (!should_execute) {
    return;  // Wait for timing or manual trigger
  }
  
  // Handle consecutive failures - reset to init if too many
  if (this->command_state_.consecutive_failures >= CommandState::MAX_FAILURES) {
    ESP_LOGW(TAG, "Too many consecutive failures (%d), resetting to INIT_1", 
             this->command_state_.consecutive_failures);
    this->state_ = STATE_INIT_1;
    this->command_state_.consecutive_failures = 0;
    this->init_complete_ = false;
    this->ac_state_known_ = false;
  }
  
  // State machine implementation
  if (!this->sender_) {
    ESP_LOGW(TAG, "Sender not available, skipping state execution");
    return;
  }
  
  switch (this->state_) {
    case STATE_INIT_1:
      ESP_LOGD(TAG, "Executing STATE_INIT_1 - sending init message 1");
      this->sender_->send_init_message_1();
      this->last_state_time_ = now;
      this->command_state_.reply_received = false;
      break;
      
    case STATE_INIT_2:
      ESP_LOGD(TAG, "Executing STATE_INIT_2 - sending init message 2");
      this->sender_->send_init_message_2();
      this->last_state_time_ = now;
      this->command_state_.reply_received = false;
      break;
      
    case STATE_INIT_3:
      ESP_LOGD(TAG, "Executing STATE_INIT_3 - sending init message 3");
      this->sender_->send_init_message_3();
      this->last_state_time_ = now;
      this->command_state_.reply_received = false;
      break;
      
    case STATE_WIFI_STATUS:
      // Check if we have a queued HA command - it takes precedence
      if (this->command_state_.pending) {
        ESP_LOGD(TAG, "Executing queued HA command instead of wifi status");
        this->sender_->send_prepared_command();  // Send the prepared command
        this->command_state_.pending = false;
        this->sender_->clear_command_buffer();  // Clear command buffer
      } else {
        ESP_LOGD(TAG, "Executing STATE_WIFI_STATUS - sending wifi status");
        this->sender_->send_wifi_status();
      }
      this->last_state_time_ = now;
      this->command_state_.reply_received = false;
      break;
      
    case STATE_POLLING:
      // Check if we have a queued HA command - it takes precedence
      if (this->command_state_.pending) {
        ESP_LOGD(TAG, "Executing queued HA command instead of status poll");
        this->sender_->send_prepared_command();  // Send the prepared command
        this->command_state_.pending = false;
        this->sender_->clear_command_buffer();  // Clear command buffer
      } else if (!this->ac_state_known_) {
        ESP_LOGD(TAG, "Executing STATE_POLLING - first status request");
        this->sender_->send_status_request();
      } else {
        ESP_LOGD(TAG, "Executing STATE_POLLING - regular status request");
        this->sender_->send_status_request();
      }
      this->last_state_time_ = now;
      this->command_state_.reply_received = false;
      break;
  }
}

void HisenseAC::dump_config() {
  ESP_LOGCONFIG(TAG, "Hisense AC:");
  ESP_LOGCONFIG(TAG, "  UART Device: Available");
  ESP_LOGCONFIG(TAG, "  ProtocolDecoder sent: %d", this->messages_sent_);
  ESP_LOGCONFIG(TAG, "  ProtocolDecoder received: %d", this->messages_received_);
  ESP_LOGCONFIG(TAG, "  CRC errors: %d", this->crc_errors_);
}

void HisenseAC::on_uart_message(const std::vector<uint8_t>& data) {
  // Force setup if not already done
  if (!this->decoder_ && !this->sender_) {
    ESP_LOGI(TAG, "Forcing component setup from on_uart_message");
    this->setup();
  }
  
  // Safety check for decoder with detailed logging
  ESP_LOGV(TAG, "Checking decoder pointer: %p", this->decoder_);
  if (!this->decoder_) {
    ESP_LOGE(TAG, "Decoder not initialized! Component state: init_complete=%s, ac_state_known=%s, state=%d", 
             this->init_complete_ ? "YES" : "NO", 
             this->ac_state_known_ ? "YES" : "NO", 
             (int)this->state_);
    ESP_LOGE(TAG, "Component address: %p, sender address: %p", this, this->sender_);
    return;
  }
  
  ESP_LOGD(TAG, "Decoder is valid, proceeding with message validation");
  
  // Validate message using the decoder
  auto validation_result = this->decoder_->validate_message(data);
  if (validation_result.valid) {
    ESP_LOGD(TAG, "Valid message received - processing and updating state machine");
    
    // Mark that we received a reply
    this->command_state_.reply_received = true;
    this->command_state_.consecutive_failures = 0;  // Reset failure counter
    this->last_message_time_ = millis();
    this->messages_received_++;
    
  // Decode the message - only handle Command and StatusRequest replies
  bool is_command_reply = (validation_result.msg_packet_type == ProtocolDecoder::MessageTypes::Command::TYPE && 
    validation_result.msg_sub_type == ProtocolDecoder::MessageTypes::Command::SUB_TYPE);
  bool is_status_reply = (validation_result.msg_packet_type == ProtocolDecoder::MessageTypes::StatusRequest::TYPE && 
  validation_result.msg_sub_type == ProtocolDecoder::MessageTypes::StatusRequest::SUB_TYPE);

  if (is_command_reply) {
  ESP_LOGD(TAG, "Decoding command reply message (0x%02X/0x%02X)", 
  ProtocolDecoder::MessageTypes::Command::TYPE, ProtocolDecoder::MessageTypes::Command::SUB_TYPE);
  this->decoder_->decode_status_message(data);
  } else if (is_status_reply) {
  ESP_LOGD(TAG, "Decoding status reply message (0x%02X/0x%02X)", 
  ProtocolDecoder::MessageTypes::StatusRequest::TYPE, ProtocolDecoder::MessageTypes::StatusRequest::SUB_TYPE);
  this->decoder_->decode_status_message(data);
  } else {
  // Log and ignore other message types
  ESP_LOGV(TAG, "Ignoring message type 0x%02X/0x%02X", 
  validation_result.msg_packet_type, validation_result.msg_sub_type);
  }
    
    // Handle state transitions based on current state
    switch (this->state_) {
      case STATE_INIT_1:
        ESP_LOGI(TAG, "Init 1 reply received - advancing to INIT_2");
        this->state_ = STATE_INIT_2;
        break;
        
      case STATE_INIT_2:
        ESP_LOGI(TAG, "Init 2 reply received - advancing to INIT_3");
        this->state_ = STATE_INIT_3;
        break;
        
      case STATE_INIT_3:
        ESP_LOGI(TAG, "Init 3 reply received - advancing to WIFI_STATUS, initialization complete");
        this->state_ = STATE_WIFI_STATUS;
        this->init_complete_ = true;
        break;
        
      case STATE_WIFI_STATUS:
        ESP_LOGD(TAG, "Wifi status reply received - advancing to POLLING");
        this->state_ = STATE_POLLING;
        break;
        
      case STATE_POLLING:
        ESP_LOGD(TAG, "Status reply received - advancing to WIFI_STATUS");
        this->state_ = STATE_WIFI_STATUS;
        this->ac_state_known_ = true;  // Mark that we know the AC state
        break;
    }
    
    // Publish state update to ESPHome
    this->publish_state();
    
  } else {
    ESP_LOGW(TAG, "Invalid message received (CRC or format error)");
    this->crc_errors_++;
    this->command_state_.consecutive_failures++;
    
    ESP_LOGW(TAG, "Consecutive failures: %d/%d", 
             this->command_state_.consecutive_failures, CommandState::MAX_FAILURES);
  }
}

climate::ClimateTraits HisenseAC::traits() {
  auto traits = climate::ClimateTraits();
  
  // Supported modes
  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_AUTO,  // Auto
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_FAN_ONLY
  });
  
  // Supported fan modes - using custom fan modes for all AC levels
  traits.set_supported_fan_modes({
    climate::CLIMATE_FAN_OFF,      // Level 0: Fan off
    climate::CLIMATE_FAN_AUTO,     // Level 1: Auto
    climate::CLIMATE_FAN_LOW,      // Level 10: Low
    climate::CLIMATE_FAN_MEDIUM,   // Level 14: Medium  
    climate::CLIMATE_FAN_HIGH      // Level 18: High
  });
  
  // Add custom fan modes for the intermediate levels
  traits.set_supported_custom_fan_modes({
    "Quiet",      // Level 2: Quiet mode
    "Med-Low",    // Level 12: Medium low
    "Med-High"    // Level 16: Medium high
  });
  
  // Supported swing modes
  traits.set_supported_swing_modes({
    climate::CLIMATE_SWING_OFF,
    climate::CLIMATE_SWING_VERTICAL,
    climate::CLIMATE_SWING_HORIZONTAL,
    climate::CLIMATE_SWING_BOTH
  });
  
  // Temperature range (from protocol documentation)
  traits.set_visual_min_temperature(16);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(1);
  
  traits.set_supports_current_temperature(true);
  traits.set_supports_two_point_target_temperature(false);
  
  return traits;
}

void HisenseAC::control(const climate::ClimateCall &call) {
  // Only process control commands if initialization is complete
  if (!this->init_complete_) {
    ESP_LOGW(TAG, "Control called but initialization not complete, ignoring");
    return;
  }
  
  // Track what changed for smart command generation
  std::vector<std::string> changes;
  
  // Handle mode changes
  if (call.get_mode().has_value()) {
    climate::ClimateMode new_mode = *call.get_mode();
    if (new_mode != this->mode) {
      this->mode = new_mode;
      this->current_mode_ = new_mode;
      changes.push_back("mode");
      ESP_LOGD(TAG, "Mode changed to: %d", (int)new_mode);
    }
  }
  
  // Handle temperature changes
  if (call.get_target_temperature().has_value()) {
    uint8_t new_temp = *call.get_target_temperature();
    if (abs(new_temp - this->target_temperature) > 0) {
      this->target_temperature = new_temp;
      this->target_temp_ = new_temp;
      changes.push_back("temperature");
      ESP_LOGD(TAG, "Target temperature changed to: %d", new_temp);
    }
  }
  
  // Handle fan mode changes (standard modes)
  if (call.get_fan_mode().has_value()) {
    climate::ClimateFanMode new_fan = *call.get_fan_mode();
    if (new_fan != this->fan_mode) {
      this->fan_mode = new_fan;
      this->current_fan_mode_ = new_fan;
      this->current_custom_fan_mode_ = "";  // Clear custom mode when using standard
      changes.push_back("fan_mode");
      ESP_LOGD(TAG, "Fan mode changed to: %d", (int)new_fan);
    }
  }
  
  // Handle custom fan mode changes
  if (call.get_custom_fan_mode().has_value()) {
    std::string new_custom_fan = *call.get_custom_fan_mode();
    if (new_custom_fan != this->current_custom_fan_mode_) {
      this->current_custom_fan_mode_ = new_custom_fan;
      this->custom_fan_mode = std::string(new_custom_fan);
      this->current_fan_mode_ = climate::CLIMATE_FAN_ON;
      this->fan_mode = climate::CLIMATE_FAN_ON;
      changes.push_back("fan_mode");
      ESP_LOGD(TAG, "Custom fan mode changed to: %s", new_custom_fan.c_str());
    }
  }
  
  // Handle swing mode changes
  if (call.get_swing_mode().has_value()) {
    climate::ClimateSwingMode new_swing = *call.get_swing_mode();
    if (new_swing != this->swing_mode) {
      this->swing_mode = new_swing;
      this->current_swing_mode_ = new_swing;
      changes.push_back("swing_mode");
      ESP_LOGD(TAG, "Swing mode changed to: %d", (int)new_swing);
    }
  }
  
  // Build chainable command for state machine to send
  if (!changes.empty() && this->sender_) {
    ESP_LOGD(TAG, "Changes detected: %zu properties - building chainable command", changes.size());
    
    // Apply individual property setters only for changed properties
    for (const auto& property : changes) {
      if (property == "temperature") {
        this->sender_->set_temperature(this->target_temp_);
        ESP_LOGD(TAG, "Set temperature bits: %d°C", this->target_temp_);
      } else if (property == "mode") {
        if (!this->run_status_) {
          this->sender_->set_power(true);  // Ensure power is on if not already
          this->run_status_ = true;  // Update run status
          ESP_LOGD(TAG, "Power turned ON for mode change"); 
        } else if (this->current_mode_ == climate::CLIMATE_MODE_OFF) {
          this->sender_->set_power(false);  // Turn off power if mode is OFF
          this->run_status_ = false;  // Update run status
          ESP_LOGD(TAG, "Power turned OFF for mode change");
        }
        this->sender_->set_mode(this->current_mode_);
        ESP_LOGD(TAG, "Set mode bits: %d", (int)this->current_mode_);
      } else if (property == "fan_mode") {
          if (this->current_custom_fan_mode_.empty()) {
            // Use standard fan mode
            this->sender_->set_fan_mode(this->current_fan_mode_);
          } else {
            // Use custom fan mode
          this->sender_->set_fan_mode(this->current_custom_fan_mode_);
          }
        ESP_LOGD(TAG, "Set fan bits: %d, custom: %s", (int)this->current_fan_mode_, this->current_custom_fan_mode_.c_str());
      } else if (property == "swing_mode") {
        this->sender_->set_swing_mode(this->current_swing_mode_);
        ESP_LOGD(TAG, "Set swing bits: %d", (int)this->current_swing_mode_);
      }
    }
    
    // Mark command as pending for state machine to pick up
    this->command_state_.pending = true;
    
    this->update_climate_state();
  } else if (!changes.empty() && !this->sender_) {
    ESP_LOGE(TAG, "Command sender not initialized!");
  }
  
  this->publish_state();
}

void HisenseAC::set_power_state(bool power_on) {
  if (this->run_status_ != power_on) {
    this->run_status_ = power_on;
    if (this->init_complete_) {
      this->sender_->set_power(power_on);
      ESP_LOGD(TAG, "Power state set to: %s", power_on ? "ON" : "OFF");
      this->command_state_.pending = true;
    }
  }
}

void HisenseAC::set_display_status(bool display_on) {
  if (this->display_on_ != display_on) {
    this->display_on_ = display_on;
    // Queue a display command if we're past init state
    if (this->init_complete_) {
      this->sender_->set_display_status(display_on);
      this->command_state_.pending = true;
      ESP_LOGD(TAG, "Display command queued: %s", display_on ? "ON" : "OFF");
    }
  }
}

void HisenseAC::set_boost_mode(bool enabled) {
  if (this->boost_mode_ != enabled) {
    this->boost_mode_ = enabled;
    if (this->init_complete_) {
      this->sender_->set_boost_mode(enabled);
      ESP_LOGD(TAG, "Boost mode set to: %s", enabled ? "ON" : "OFF");
      this->command_state_.pending = true;
    }
  }
}

void HisenseAC::set_eco_mode(bool enabled) {
  if (this->eco_mode_ != enabled) {
    this->eco_mode_ = enabled;
    if (this->init_complete_) {
      this->sender_->set_eco_mode(enabled);
      ESP_LOGD(TAG, "Eco mode set to: %s", enabled ? "ON" : "OFF");
      this->command_state_.pending = true;
    }
  }
}

void HisenseAC::set_quiet_mode(bool enabled) {
  if (this->quiet_mode_ != enabled) {
    this->quiet_mode_ = enabled;
    if (this->init_complete_) {
      this->sender_->set_quiet_mode(enabled);
      ESP_LOGD(TAG, "Quiet mode set to: %s", enabled ? "ON" : "OFF");
      this->command_state_.pending = true;
    }
  }
}

void HisenseAC::set_sleep_enabled(bool enabled) {
  if (this->sleep_enabled_ != enabled) {
    this->sleep_enabled_ = enabled;
    if (this->init_complete_) {
      if (enabled) {
        // Set preferred sleep mode when enabling sleep
        this->sender_->set_sleep_mode(this->preferred_sleep_mode_);
      } else {
        // Disable sleep mode
        this->sender_->set_sleep_mode(SleepMode::SLEEP_OFF);
      }
      ESP_LOGD(TAG, "Sleep mode set to: %s", enabled ? "ON" : "OFF");
      this->command_state_.pending = true;
    }
  }
}

void HisenseAC::set_preferred_sleep_mode(const std::string &status_str) {
  // Convert string to SleepMode enum using utility function
  SleepMode status = SleepModeUtils::from_string(status_str);
  set_preferred_sleep_mode(status);
}

void HisenseAC::set_preferred_sleep_mode(SleepMode status) {
  if (status == SleepMode::SLEEP_OFF) return;  // Ignore if sleep is off

  // Only update if the status has changed
  if (this->preferred_sleep_mode_ != status) {
    this->preferred_sleep_mode_ = status;
    if (this->init_complete_) {
      if (this->sleep_enabled_) {
        // If sleep is enabled, set the preferred sleep mode
        this->sender_->set_sleep_mode(status);
        this->command_state_.pending = true;
      }
      ESP_LOGD(TAG, "Preferred sleep mode set to: %s", SleepModeUtils::to_string(status));
    }
  }
}

void HisenseAC::update_climate_state() {
  mode = current_mode_;
  fan_mode = current_fan_mode_;
  swing_mode = current_swing_mode_;
  target_temperature = target_temp_;
  current_temperature = current_temp_;

  ESP_LOGD(TAG, "Climate state updated - Mode: %s, Temp: %d°C", 
           this->get_mode_string(this->current_mode_).c_str(), this->target_temp_);
}

std::string HisenseAC::get_mode_string(climate::ClimateMode mode) {
  switch (mode) {
    case climate::CLIMATE_MODE_OFF: return "Off";
    case climate::CLIMATE_MODE_AUTO: return "Auto";
    case climate::CLIMATE_MODE_COOL: return "Cool";
    case climate::CLIMATE_MODE_HEAT: return "Heat";
    case climate::CLIMATE_MODE_DRY: return "Dry";
    case climate::CLIMATE_MODE_FAN_ONLY: return "Fan Only";
    default: return "Unknown";
  }
}

// Sleep utility functions moved to SleepModeUtils in common_types.h

}  // namespace hisense_ac
}  // namespace esphome
