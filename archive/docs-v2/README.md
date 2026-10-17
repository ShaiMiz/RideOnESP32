# ESP32 Power Wheels Controller Firmware

## 📋 Project Overview

This firmware replaces the original control board in a **Power Wheels Can-Am Maverick X3** toy with an ESP32-based system that provides both manual (pedal/shifter) and remote control (ELRS radio) capabilities.

### **Hardware Specifications**
- **Battery**: 24V (original Can-Am X3 batteries)
- **Motors**: 4x 24V 22k RPM 200W motors
- **Motor Drivers**: 4x BTS7960 H-Bridge (43A continuous, 100A peak)
- **Radio**: Jumper T-Lite V2 2.4GHz ELRS + ELRS Nano Receiver
- **Protection**: XH-M609 battery discharge protection + 30A circuit breaker
- **Power Supply**: LM2596 buck converters (3.3V for ESP32, 12V for accessories)

**Author:** [Your Name]  
**Version:** 2.1  
**Last Updated:** December 2025

---

## ✨ Features

### Control Modes
- **Manual Mode**: Traditional Power Wheels operation using pedal and physical shifter (Drive/Park/Reverse)
- **Remote Mode**: Radio control via CRSF receiver (ELRS system) with 3-position speed mode

### Safety Systems
- ✅ **Low Voltage Cutoff**: Protects battery from over-discharge
- ✅ **Overcurrent Protection**: Prevents motor/driver damage from excessive current
- ✅ **Thermal Protection**: Monitors motor driver temperature with automatic shutdown
- ✅ **Radio Failsafe**: Emergency stop if signal lost (1 second timeout)
- ✅ **Motor Ramping**: Gradual acceleration/deceleration to prevent gear damage
- ✅ **Park Mode Enforcement**: Motors disabled when shifter in PARK position

### Additional Features
- 4-wheel independent motor control
- Lights control (manual always-on, remote switchable)
- Real-time telemetry output via Serial
- Comprehensive error logging and status monitoring

---

## 🔧 Hardware Requirements

### Main Components
- **ESP32-WROOM-32U** Microcontroller (or compatible ESP32 board)
- **CRSF Receiver**: ELRS Nano 2.4GHz receiver (ExpressLRS system)
- **Transmitter**: Jumper T-Lite V2 2.4GHz with ELRS firmware
- **Motor Drivers**: **4x BTS7960 H-Bridge** modules (43A continuous, 100A peak each)
- **Motors**: 4x 24V 22k RPM 200W motors (original Can-Am X3)
- **Battery**: **24V** (original Can-Am X3 batteries - lead-acid or LiPo 6S)
- **Power Supply**: 2x LM2596 buck converters (3.3V for ESP32, 12V for accessories)
- **Protection**: XH-M609 battery discharge protection module + 30A circuit breaker
- **Current Sensor**: ACS712-30B or BTS7960 built-in current sense (optional but recommended)

### Optional Components
- Voltage divider for battery monitoring (82kΩ + 12kΩ resistors)
- LED lights/strips
- DFPlayer Mini or similar MP3 module for audio (not yet implemented)

---

## 📌 Pin Connections

### Motor Control
| Function | GPIO Pin | Description |
|----------|----------|-------------|
| FL_PWM | 25 | Front-left motor PWM |
| FL_DIR | 26 | Front-left motor direction |
| FR_PWM | 27 | Front-right motor PWM |
| FR_DIR | 14 | Front-right motor direction |
| RL_PWM | 12 | Rear-left motor PWM |
| RL_DIR | 13 | Rear-left motor direction |
| RR_PWM | 32 | Rear-right motor PWM |
| RR_DIR | 33 | Rear-right motor direction |
| MOTOR_ENABLE | 21 | Global enable for all motor drivers |

### Telemetry (ADC1 Pins)
| Function | GPIO Pin | Description |
|----------|----------|-------------|
| VOLTAGE_PIN | 35 | Battery voltage (via 82k/12k divider) |
| CURRENT_PIN | 36 | Motor current sensor (ACS712) |
| TEMP_FAULT_PIN | 19 | Over-temp fault (active LOW) |

### Manual Control Inputs
| Function | GPIO Pin | Description |
|----------|----------|-------------|
| PEDAL_PIN | 34 | Analog pedal potentiometer (0-3.3V) |
| SHIFTER_D_PIN | 39 | Drive position (active LOW) |
| SHIFTER_R_PIN | 5 | Reverse position (active LOW) |

### Lights & Audio
| Function | GPIO Pin | Description |
|----------|----------|-------------|
| LIGHT_PIN | 22 | Lights control (HIGH=on) |
| AUDIO_PLAY_PIN | 23 | Audio module play control |
| AUDIO_VOLUP_PIN | 18 | Audio module volume up |
| AUDIO_VOLDOWN_PIN | 4 | Audio module volume down |

### CRSF Receiver (UART2)
| Function | GPIO Pin | Description |
|----------|----------|-------------|
| RX | 16 | CRSF receiver TX output |
| TX | 17 | CRSF receiver RX input |

---

## 📚 Software Requirements

### Arduino IDE Setup
1. Install **Arduino IDE** 2.x or later
2. Install **ESP32 board support** via Board Manager
   - URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Select board: **ESP32 Dev Module** or your specific board

### Required Libraries
Install via Arduino Library Manager:
- **AlfredoCRSF**: For CRSF receiver communication
  - GitHub: https://github.com/AlfredoSystems/AlfredoCRSF
  - Install via: Sketch → Include Library → Manage Libraries → Search "AlfredoCRSF"

### Board Configuration
In Arduino IDE, select:
- **Board**: "ESP32 Dev Module" (or your specific board)
- **Upload Speed**: 921600
- **CPU Frequency**: 240MHz
- **Flash Frequency**: 80MHz
- **Flash Mode**: QIO
- **Flash Size**: 4MB (or your board's flash size)
- **Partition Scheme**: Default 4MB with spiffs
- **Core Debug Level**: None (or Info for debugging)
- **Port**: Your ESP32's COM port

---

## 🚀 Installation

1. **Wire Hardware**: Connect ESP32 to motor drivers, sensors, and receiver per pin table above
2. **Install Libraries**: Install AlfredoCRSF library in Arduino IDE
3. **Configure Settings**: Edit `sketch_jun22b.ino` constants for your setup:
   ```cpp
   // Adjust these for your battery type:
   const float LOW_VOLTAGE_CUTOFF = 10.5;  // 2S LiPo: 6.0V, 3S: 9.0V, 4S: 12.0V
   
   // Adjust for your current sensor:
   const float CURRENT_SENSOR_ZERO = 2.5;    // ACS712 zero point
   const float CURRENT_SENSITIVITY = 0.185;  // ACS712-05B: 0.185 V/A
   const float MAX_CURRENT_LIMIT = 15.0;     // Your safe maximum
   
   // Adjust voltage divider if different:
   const float VOLTAGE_DIVIDER_RATIO = 7.833; // (82k + 12k) / 12k
   ```
4. **Upload Firmware**: Connect ESP32 via USB and upload `sketch_jun22b.ino`
5. **Open Serial Monitor**: Set baud rate to **115200** to view telemetry
6. **Test System**: Follow testing procedure below

---

## 🧪 Testing Procedure

### 1. Bench Testing (Motors Disconnected)
1. Set `#define DEVELOPMENT_MODE true` in code (line ~144)
2. Upload and open Serial Monitor (115200 baud)
3. Verify:
   - Setup messages appear
   - PWM channels initialize successfully
   - CRSF receiver connects
   - Mode switching works

### 2. Motor Testing (No Load)
1. Set `#define DEVELOPMENT_MODE false`
2. Connect motors (wheels off ground or disconnected)
3. Test manual mode:
   - Shifter in PARK → motors should NOT spin
   - Shifter in DRIVE + pedal pressed → motors spin forward (slowly)
   - Shifter in REVERSE + pedal → motors spin reverse
4. Test remote mode:
   - Switch to remote mode on transmitter
   - Verify throttle control works
   - Test 3-position speed switch (Reverse/Low/Fast)
   - Test lights toggle

### 3. Safety System Testing
1. Monitor Serial output for telemetry
2. Test low voltage cutoff:
   - Reduce battery voltage below threshold
   - Verify motors disable and warning appears
3. Test radio failsafe:
   - Turn off transmitter
   - Verify motors stop within 1 second
4. Test temperature protection (if sensor connected)

### 4. Full System Test
1. Place vehicle on ground
2. Test manual mode operation
3. Test remote mode operation
4. Verify smooth acceleration (ramping working)
5. Check all safety systems remain active

---

## 🎛️ CRSF Channel Configuration

Configure your ELRS transmitter with these channel assignments:

| Channel | Function | Type | Range |
|---------|----------|------|-------|
| 0 | Throttle | Stick (vertical) | 1000-2000 |
| 1 | Steering | Stick (horizontal) | 1000-2000 |
| 2 | Horn | Momentary switch | >1500 = active |
| 3 | Mode Switch | 2-position switch | <1500 = Manual, >1500 = Remote |
| 5 | Lights | 2-position switch | <1500 = Off, >1500 = On |
| 6 | Speed Mode | 3-position switch | <1300 = Reverse, 1300-1700 = Low, >1700 = Fast |

**Note**: Steering is read but not currently used (implementation pending).

---

## ⚙️ Configuration Guide

### Adjusting Safety Thresholds

Edit these constants in `sketch_jun22b.ino` (around line 80-110):

```cpp
// Battery protection (adjust for your battery type)
const float LOW_VOLTAGE_CUTOFF = 10.5;     // Shutdown voltage
const float LOW_VOLTAGE_WARNING = 11.0;    // Warning voltage

// Current protection (adjust for your motors)
const float MAX_CURRENT_LIMIT = 15.0;      // Emergency shutdown
const float HIGH_CURRENT_WARNING = 12.0;   // Warning threshold

// Radio failsafe (adjust for reliability)
const unsigned long RADIO_TIMEOUT_MS = 1000; // 1 second

// Motor ramping (adjust for responsiveness)
const int RAMP_STEP = 10;        // Larger = faster acceleration
const int RAMP_DELAY_MS = 20;    // Smaller = smoother ramping
```

### Voltage Divider Calculation

If using different resistors for battery monitoring:
```cpp
// Formula: RATIO = (R1 + R2) / R2
// Example: 82kΩ and 12kΩ resistors
const float VOLTAGE_DIVIDER_RATIO = (82.0 + 12.0) / 12.0; // = 7.833
```

### Current Sensor Calibration

For different current sensors:
```cpp
// ACS712-05B (±5A):  0.185 V/A
// ACS712-20B (±20A): 0.100 V/A
// ACS712-30B (±30A): 0.066 V/A
const float CURRENT_SENSITIVITY = 0.185; // Change per your sensor
```

---

## 🐛 Troubleshooting

### Motors Don't Move
- Check Serial Monitor for error messages
- Verify `motorsEnabled = true` in telemetry output
- Check MOTOR_ENABLE pin is HIGH
- Verify PWM initialization succeeded (no errors in setup)
- Test with `DEVELOPMENT_MODE = true` to bypass safety checks

### Motors Spin Wrong Direction
- Option 1: Swap motor wires (+ and -)
- Option 2: Invert direction logic in `setMotorDirection()` function

### Radio Signal Issues
- Verify CRSF receiver powered and bound to transmitter
- Check RX/TX pins (16, 17) connected correctly
- Confirm baud rate is 420000 (CRSF standard)
- Monitor Serial for "Radio signal lost" messages

### Voltage Reading Incorrect
- Verify voltage divider resistor values
- Check VOLTAGE_DIVIDER_RATIO constant matches hardware
- Measure voltage at GPIO 35 (should be <3.3V)
- Test with known battery voltage

### Current Reading Incorrect
- Verify current sensor connections (5V, GND, OUT)
- Check CURRENT_SENSOR_ZERO and CURRENT_SENSITIVITY
- Calibrate: Read ADC with 0A flowing, adjust ZERO value

---

## 📊 Serial Monitor Output Example

```
[0 ms] ESP32 Power Wheels Controller Starting...
[105 ms] CRSF receiver initialized on Serial2 (RX:16, TX:17, 420000 baud)
[110 ms] ADC configured: 12-bit resolution (0-4095)
[115 ms] Motor drivers enabled (MOTOR_ENABLE = HIGH)
[120 ms] Motor direction pins initialized
[145 ms] Motor PWM initialized: 1kHz, 8-bit (0-255)
[150 ms] Telemetry and control inputs configured
[155 ms] Steering motor pins configured (PWM not initialized)
[160 ms] Lights and audio pins configured
[165 ms] ========================================
[170 ms] Setup complete. System ready.
[175 ms] Mode: Waiting for input...
[180 ms] ========================================
[1000 ms] Status: MANUAL | Lights: ON | Shifter: PARK
[1005 ms] Telemetry: Battery=12.45V | Current=0.15A | Motors=ENABLED | PWM=0/0
```

---

## 🔜 Future Enhancements

### Planned Features
- [ ] **Steering Motor Control**: Implement power steering using STEERING_CH
- [ ] **Audio System**: Engine sounds, horn, startup sounds via DFPlayer Mini
- [ ] **Speed Limiting**: Configurable max speed via potentiometer or switch
- [ ] **Battery Percentage**: Calculate and display remaining battery capacity
- [ ] **Over-Air Updates**: WiFi-based firmware updates
- [ ] **Data Logging**: SD card logging of telemetry for analysis
- [ ] **LED Status Indicators**: Multi-color LED for system status

### Known Limitations
- Steering input read but not used (waiting for power steering implementation)
- Audio pins configured but no audio control logic
- No horn sound implementation (requires audio system)
- No WiFi/Bluetooth features (code focused on motor control)

---

## 🔐 Safety Notes

⚠️ **IMPORTANT SAFETY WARNINGS** ⚠️

1. **Always test with motors disconnected first** - Verify control logic before applying power to motors
2. **Use appropriate fuses** - Add fuses to battery connections to prevent fire hazard
3. **Check voltage thresholds** - Adjust LOW_VOLTAGE_CUTOFF for your battery type
4. **Monitor current draw** - Ensure your motor drivers can handle the current
5. **Test failsafe** - Verify radio failsafe works before giving vehicle to child
6. **Supervise operation** - Adult supervision required when vehicle is in use
7. **Check mechanical systems** - Ensure gears, wheels, and steering are in good condition
8. **Avoid water** - ESP32 and motor drivers are NOT waterproof

---

## 📝 License

This project is provided as-is for educational and personal use. 

**Disclaimer**: This firmware involves high-current motors and batteries. The author is not responsible for any damage, injury, or issues resulting from use of this code. Use at your own risk and ensure proper safety precautions.

---

## 🤝 Contributing

Improvements and bug fixes are welcome! If you enhance this firmware:
1. Fork the repository
2. Make your changes
3. Test thoroughly
4. Submit a pull request with detailed description

---

## 📧 Support

For questions or issues:
1. Check the Troubleshooting section above
2. Review Serial Monitor output for error messages
3. Verify all hardware connections
4. Create an issue on GitHub with:
   - Description of problem
   - Serial Monitor output
   - Hardware configuration
   - Steps to reproduce

---

## 🙏 Acknowledgments

- **AlfredoSystems** for the AlfredoCRSF library
- **Espressif** for the ESP32 platform and Arduino core
- **ELRS Team** for ExpressLRS radio system
- **Power Wheels Community** for inspiration and mods

---

**Happy Riding! 🚗💨**

