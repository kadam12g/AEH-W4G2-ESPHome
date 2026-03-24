*This is an AI generated placeholder README. Proper readme, flashing instructions and tips are coming whenever I have time to work on it.*

# Hisense AC ESPHome Component

A comprehensive ESPHome custom component for controlling Hisense air conditioners via UART communication.

## Features

- Full climate control (temperature, mode, fan speed, swing)
- Special modes (Eco, Boost, Quiet, Sleep)
- Real-time status monitoring
- Multiple temperature sensors
- Timer controls
- Display control
- Comprehensive error handling and retry logic

## Hardware Requirements

- ESP32 or ESP8266 microcontroller
- UART connection to Hisense AC unit
- Optional: Flow control GPIO pin

## Project Structure

```
ESPHome/
├── my_components/
│   └── hisense_ac/
│       ├── __init__.py          # ESPHome component initialization
│       ├── climate.py           # ESPHome climate platform
│       ├── common_types.h       # Shared enums and utilities
│       ├── hisense_ac.h/cpp     # Main component class
│       ├── command_sender.h/cpp # Command encoding and transmission
│       └── protocol_decoder.h/cpp # Message validation and decoding
├── HISENSE_AC_PROTOCOL_DOCUMENTATION.md
└── example configurations...
```

## Installation

1. Copy the `custom_components/hisense_ac/` directory to your ESPHome configuration
2. Add the component to your ESPHome YAML configuration:

```yaml
uart:
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

climate:
  - platform: hisense_ac
    name: "Hisense AC"
    flow_control_pin: GPIO4  # Optional
```

## Protocol Documentation

See `HISENSE_AC_PROTOCOL_DOCUMENTATION.md` for complete protocol details, including:
- Message structure and encoding
- Command formats
- Status field mappings
- CRC calculation
- Initialization sequences

## Development

### Architecture

The component uses a clean separation of concerns:
- **HisenseAC**: Main ESPHome integration and state management
- **CommandSender**: Handles command construction and bit manipulation
- **ProtocolDecoder**: Validates and parses incoming messages
- **common_types.h**: Shared enums and utility functions

### State Machine

Simple linear state progression:
1. `INIT_1` → `INIT_2` → `INIT_3` (Initialization)
2. `WIFI_STATUS` ↔ `POLLING` (Normal operation)
3. Command injection during WiFi status or polling states

### Building and Testing

The component compiles with ESPHome's standard build system. Enable verbose logging for debugging:

```yaml
logger:
  level: VERBOSE
  logs:
    hisense_ac: VERBOSE
```

## Contributing

This project represents extensive reverse engineering of the Hisense AC protocol. Contributions for additional features, bug fixes, and protocol improvements are welcome.

## License

[Add your license here]

## Credits

Developed through reverse engineering efforts and protocol analysis of Hisense AEH-W4G2 air conditioning units.
