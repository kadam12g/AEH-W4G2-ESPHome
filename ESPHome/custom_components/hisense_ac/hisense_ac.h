#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"
#include "common_types.h"
#include <vector>
#include <string>

namespace esphome {
namespace hisense_ac {

// Forward declarations
class ProtocolDecoder;
class CommandSender;
class StateMachine;

class HisenseAC : public uart::UARTDevice, public Component, public climate::Climate {
 public:
  // ===== ENUMS AND CONSTANTS =====

  enum State {
    STATE_INIT_1,
    STATE_INIT_2, 
    STATE_INIT_3,
    STATE_WIFI_STATUS,
    STATE_POLLING
  };


  // ===== LIFECYCLE METHODS =====
  
  HisenseAC();
  ~HisenseAC();
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  // ===== CLIMATE INTERFACE =====
  
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  // ===== COMMUNICATION =====
  
  // Called by UART when message received
  void on_uart_message(const std::vector<uint8_t>& data);

  // Setting flow control
  void set_flow_control_pin(GPIOPin *flow_control_pin) {
    this->flow_control_pin_ = flow_control_pin;
  }
  
  // ===== CONTROL METHODS =====

  
  // Debug/manual control methods
  void set_automatic_execution(bool enabled) { automatic_execution_ = enabled; }
  void trigger_manual_step() { manual_step_requested_ = true; }
  bool get_automatic_execution() const { return automatic_execution_; }

  // Device control
  void set_buzzer_enabled(bool enabled) { beep_enabled_ = enabled;}
  void set_display_status(bool enabled);
  void set_boost_mode(bool enabled);
  void set_eco_mode(bool enabled);
  void set_quiet_mode(bool enabled);
  void set_sleep_enabled(bool enabled);
  void set_preferred_sleep_mode(const std::string &status_str);
  void set_preferred_sleep_mode(SleepMode status);
  void set_power_state(bool power_on);

  // ===== GETTERS =====
  
  // Basic state
  bool is_ready() const { return this->ac_state_known_; }
  bool get_display_on() const { return display_on_; }
  uint8_t get_sleep_status() const { return static_cast<uint8_t>(this->sleep_status_); }
  uint8_t get_power_status() const { return this->run_status_; }
  SleepMode get_preferred_sleep_mode() const { return this->preferred_sleep_mode_; }

  // Temperature sensors
  int16_t get_indoor_temperature() const { return this->current_temp_; }
  int16_t get_indoor_temperature_setting() const { return this->target_temp_; }
  int16_t get_indoor_pipe_temperature() const { return this->indoor_pipe_temperature_; }
  int16_t get_outdoor_temperature() const { return this->outdoor_temperature_; }
  int16_t get_outdoor_condenser_temperature() const { return this->outdoor_condenser_temperature_; }
  int16_t get_compressor_exhaust_temperature() const { return this->compressor_exhaust_temperature_; }
  
  // Environmental sensors
  uint8_t get_indoor_humidity_setting() const { return this->indoor_humidity_setting_; }
  uint8_t get_indoor_humidity_status() const { return this->indoor_humidity_status_; }
  
  // System status
  uint8_t get_fan_status() const { return this->fan_status_; }
  uint8_t get_mode_status() const { return this->mode_status_; }
  uint8_t get_direction_status() const { return this->direction_status_; }
  uint8_t get_up_down_status() const { return this->up_down_; }
  uint8_t get_left_right_status() const { return this->left_right_; }
  uint8_t get_eco_mode_status() const { return this->eco_mode_; }
  uint8_t get_boost_mode_status() const { return this->boost_mode_; }
  uint8_t get_quiet_mode_status() const { return this->quiet_mode_; }
  uint8_t get_temperature_compensation() const { return this->temperature_compensation_; }
  uint8_t get_compressor_frequency_setting() const { return this->compressor_frequency_setting_; }
  
  // Electrical measurements
  uint8_t get_iab() const { return this->IAB_; }
  uint8_t get_ibc() const { return this->IBC_; }
  uint8_t get_iuv() const { return this->IUV_; }
  uint8_t get_command_received() const { return this->command_received_; }
  uint8_t get_indoor_eeprom() const { return this->indoor_eeprom_; }

  // Timer status
  uint8_t get_poweron_hour() const { return this->poweron_hour_; }
  uint8_t get_poweron_minute() const { return this->poweron_minute_; }
  uint8_t is_poweron_enabled() const { return this->poweron_status_; }
  uint8_t get_poweroff_hour() const { return this->poweroff_hour_; }
  uint8_t get_poweroff_minute() const { return this->poweroff_minute_; }
  uint8_t is_poweroff_enabled() const { return this->poweroff_status_; }
  
  // Statistics
  uint32_t get_messages_sent() const { return this->messages_sent_; }
  uint32_t get_messages_received() const { return this->messages_received_; }
  uint32_t get_crc_errors() const { return this->crc_errors_; }

  std::string get_preferred_sleep_mode_string() const {
    return SleepModeUtils::to_string(this->preferred_sleep_mode_);
  }

  // ===== UTILITY METHODS =====
  
  // Sleep utility functions moved to SleepModeUtils in common_types.h

  // ===== PUBLIC STATE VARIABLES =====
  // (TODO: These should ideally be private with proper accessors)
  
  State state_ = STATE_INIT_1;
  uint32_t last_message_time_ = 0;
  uint32_t last_state_time_ = 0;  // When current state was entered
  uint32_t state_interval_ = 2000;  // At least 2 seconds between commands
  bool init_complete_ = false;
  bool ac_state_known_ = false;  // Have we received at least one status?
  
  // Debug/manual control
  bool automatic_execution_ = true;  // Can be controlled by switch
  bool manual_step_requested_ = false;  // Triggered by button
  
  // Command state management
  struct CommandState {
    bool pending = false; // command was sent, but not yet acknowledged
    bool queued = false;  // Command is ready to be sent
    uint32_t last_send_time = 0;
    bool reply_received = false;
    uint8_t consecutive_failures = 0;
    static const uint8_t MAX_FAILURES = 5;
  } command_state_;
  
  // Current AC state (ESPHome climate interface)
  bool power_state_ = false;
  uint8_t target_temp_ = 25;
  int16_t current_temp_ = 25;
  climate::ClimateMode current_mode_ = climate::CLIMATE_MODE_OFF;
  climate::ClimateFanMode current_fan_mode_ = climate::CLIMATE_FAN_AUTO;
  climate::ClimateSwingMode current_swing_mode_ = climate::CLIMATE_SWING_OFF;
  
  // Special modes and settings
  bool boost_mode_ = false;
  bool eco_mode_ = false;
  bool quiet_mode_ = false;
  bool display_on_ = true;
  bool beep_enabled_ = true;
  bool sleep_enabled_ = false;  // Sleep mode enabled
  
  // Custom mode tracking
  uint8_t actual_fan_level_ = 0;  // Track the actual AC fan level (0,1,2,10,12,14,16,18)
  std::string current_custom_fan_mode_ = "";  // Track custom fan mode

  // AC status variables
  uint8_t fan_status_ = 0;           // Fan speed (0-18)
  SleepMode sleep_status_ = SleepMode::SLEEP_OFF; // Current sleep status for ESPHome
  SleepMode preferred_sleep_mode_ = SleepMode::SLEEP_GENERAL; // Preferred sleep mode for ESPHome
  uint8_t mode_status_ = 0;           // AC mode (0-7)
  bool run_status_ = false;           // AC on/off
  uint8_t direction_status_ = 0;      // Swing direction (0,2)
  
  // Temperature measurements
  int16_t indoor_pipe_temperature_ = 0;      // Indoor pipe temp
  int16_t outdoor_temperature_ = 0;           // Outdoor temperature
  int16_t outdoor_condenser_temperature_ = 0; // Outdoor condenser temp
  int16_t compressor_exhaust_temperature_ = 0; // Compressor exhaust temp
  
  // Humidity and environmental
  uint8_t indoor_humidity_setting_ = 0;       // Humidity setting
  uint8_t indoor_humidity_status_ = 0;        // Current humidity
  uint8_t temperature_compensation_ = 0;      // Temperature compensation
  
  // Timer status
  uint8_t poweron_hour_ = 0;          // Power-on timer hours
  uint8_t poweron_minute_ = 0;        // Power-on timer minutes
  bool poweron_status_ = false;       // Power-on timer enabled
  uint8_t poweroff_hour_ = 0;         // Power-off timer hours
  uint8_t poweroff_minute_ = 0;       // Power-off timer minutes
  bool poweroff_status_ = false;      // Power-off timer enabled
  
  // Swing control
  bool up_down_ = false;              // Up-down swing
  bool left_right_ = false;           // Left-right swing
  
  // System diagnostics and measurements
  uint8_t compressor_frequency_setting_ = 0;   // Compressor frequency
  uint8_t IAB_ = 0;                           // Current measurement A-B
  uint8_t IBC_ = 0;                           // Current measurement B-C
  uint8_t IUV_ = 0;                           // Voltage measurement
  bool command_received_ = false;                        // Command received flag (TODO: Better name needed)
  bool indoor_eeprom_ = false;                // EEPROM status
  
  // Communication statistics
  uint32_t messages_sent_ = 0;
  uint32_t messages_received_ = 0;
  uint32_t crc_errors_ = 0;

  void update_climate_state();

 private:
  // ===== PRIVATE MEMBERS =====
  friend class CommandSender;
  friend class StateMachine;
  
  // Helper components
  ProtocolDecoder* decoder_{nullptr};
  CommandSender* sender_{nullptr};
  StateMachine* state_machine_{nullptr};
  GPIOPin* flow_control_pin_{nullptr};  // Flow control pin for UART
  
  // Private methods
  std::string get_mode_string(climate::ClimateMode mode);
};

}  // namespace hisense_ac
}  // namespace esphome