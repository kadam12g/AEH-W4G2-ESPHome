#pragma once

#include <cstdint>
#include <string>

namespace esphome {
namespace hisense_ac {

// ===== COMMON ENUMS =====

enum class SleepMode : uint8_t {
  SLEEP_OFF = 0,
  SLEEP_GENERAL = 2,
  SLEEP_OLD = 4,
  SLEEP_YOUNG = 6,
  SLEEP_KIDS = 8
};

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

// ===== UTILITY FUNCTIONS =====

class SleepModeUtils {
public:
  static const char* to_string(SleepMode mode) {
    switch (mode) {
      case SleepMode::SLEEP_OFF: return "Off";
      case SleepMode::SLEEP_GENERAL: return "Sleep (General)";
      case SleepMode::SLEEP_OLD: return "Sleep for Old";
      case SleepMode::SLEEP_YOUNG: return "Sleep for Young";
      case SleepMode::SLEEP_KIDS: return "Sleep for Kids";
      default: return "Unknown";
    }
  }
  
  static SleepMode from_string(const std::string& status_str) {
    if (status_str == "Sleep (General)") return SleepMode::SLEEP_GENERAL;
    if (status_str == "Sleep for Young") return SleepMode::SLEEP_YOUNG;
    if (status_str == "Sleep for Old") return SleepMode::SLEEP_OLD;
    if (status_str == "Sleep for Kids") return SleepMode::SLEEP_KIDS;
    return SleepMode::SLEEP_GENERAL; // Default
  }
  
  // Convert to CommandSender format (divide by 2)
  static uint8_t to_command_value(SleepMode mode) {
    return static_cast<uint8_t>(mode) / 2;
  }
  
  // Convert from CommandSender format (multiply by 2)
  static SleepMode from_command_value(uint8_t value) {
    return static_cast<SleepMode>(value * 2);
  }
};

class FanModeUtils {
public:
  static const char* to_string(FanMode mode) {
    switch (mode) {
      case FanMode::FAN_OFF: return "Off";
      case FanMode::FAN_AUTO: return "Auto";
      case FanMode::FAN_QUIET: return "Quiet";
      case FanMode::FAN_LOW: return "Low";
      case FanMode::FAN_LOW_MEDIUM: return "Med-Low";
      case FanMode::FAN_MEDIUM: return "Medium";
      case FanMode::FAN_MEDIUM_HIGH: return "Med-High";
      case FanMode::FAN_HIGH: return "High";
      default: return "Unknown";
    }
  }
  
  static FanMode from_string(const std::string& mode_str) {
    if (mode_str == "Auto") return FanMode::FAN_AUTO;
    if (mode_str == "Quiet") return FanMode::FAN_QUIET;
    if (mode_str == "Low") return FanMode::FAN_LOW;
    if (mode_str == "Med-Low") return FanMode::FAN_LOW_MEDIUM;
    if (mode_str == "Medium") return FanMode::FAN_MEDIUM;
    if (mode_str == "Med-High") return FanMode::FAN_MEDIUM_HIGH;
    if (mode_str == "High") return FanMode::FAN_HIGH;
    return FanMode::FAN_AUTO; // Default
  }
};

}  // namespace hisense_ac
}  // namespace esphome