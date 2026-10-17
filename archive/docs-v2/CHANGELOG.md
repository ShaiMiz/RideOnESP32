# Changelog

All notable changes to the ESP32 Power Wheels Controller firmware.

---

## [Version 2.0] - December 2025

### 🔧 **BREAKING CHANGES**
- **Updated to ESP32 Arduino Core 3.x API**
  - Replaced deprecated `ledcSetup()` and `ledcAttachPin()` with new `ledcAttach()` API
  - PWM now uses GPIO pin numbers directly instead of channel numbers
  - `ledcWrite()` now takes pin numbers, not channels
  - **⚠️ Code will NOT work with ESP32 Core 2.x without modifications**

- **Removed ESP32Servo library dependency**
  - Was included but never used
  - Native LEDC functions provide better control for motor drivers

### ✨ **New Features**
- **Comprehensive Safety Systems**
  - Battery voltage monitoring with low-voltage cutoff
  - Motor current monitoring with overcurrent protection
  - Over-temperature protection with auto-recovery timer
  - Radio failsafe with 1-second timeout
  - Emergency stop function for instant shutdown

- **Motor Ramping System**
  - Gradual acceleration/deceleration to prevent gear damage
  - Configurable ramp rate (step size and delay)
  - Smooth transitions between speed changes

- **Enhanced Telemetry**
  - Real-time battery voltage reading (via voltage divider)
  - Motor current monitoring (via ACS712 or similar sensor)
  - Comprehensive status output every second
  - Detailed error and warning messages

- **PARK Mode Enforcement**
  - Motors completely disabled when shifter in PARK position
  - Pedal input ignored in PARK for safety

- **Development Mode**
  - Test code without hardware sensors connected
  - Fake sensor values for bench testing
  - Enable/disable via single flag

### 📝 **Code Quality Improvements**
- **Extensive Documentation**
  - Complete header comments explaining project purpose
  - Doxygen-style function documentation for all functions
  - Inline comments explaining complex logic
  - Pin assignment descriptions with voltage/current specifications

- **Better Code Organization**
  - Separated pin definitions with clear categories
  - Safety constants grouped together
  - Runtime variables clearly documented
  - Function declarations with full documentation

- **Improved Error Handling**
  - PWM initialization checks with error reporting
  - Safety condition monitoring with detailed warnings
  - Sensor reading validation and sanity checks

### 🔄 **Changed**
- **setup() function completely rewritten**
  - Uses new `ledcAttach()` API for PWM configuration
  - Added explicit ADC resolution setting (12-bit)
  - Better initialization logging
  - Error checking for PWM setup failures

- **loop() function restructured**
  - Added safety checks before control logic
  - Separated motor ramping from speed setting
  - Better control flow with comments
  - Radio signal timestamp tracking

- **Control logic improved**
  - `remoteControlLogic()`: Better throttle mapping, speed mode handling
  - `manualControlLogic()`: PARK mode enforcement, safer pedal reading
  - Both functions now use ramping system

- **Telemetry output enhanced**
  - Shows battery voltage and current
  - Displays motor enable status
  - Shows current/target PWM values
  - More informative status messages

### 🐛 **Bug Fixes**
- **CRITICAL: Fixed PWM initialization**
  - Old code called `ledcWrite()` but never initialized PWM channels
  - Motors would not have worked at all
  - Now properly calls `ledcAttach()` for each motor pin

- **Fixed direction control**
  - Now explicitly sets HIGH/LOW instead of boolean cast
  - More reliable direction switching

- **Fixed voltage divider calculations**
  - Correct formula: (ADC / 4095) * 3.3V * divider_ratio
  - Added sanity checks for unreasonable voltages

### 📊 **Performance**
- Loop runs at ~100Hz (with 10ms delay)
- Motor ramping updates every 20ms (50Hz)
- Telemetry output every 1 second
- Safety checks every loop iteration

### 🔐 **Security/Safety**
- All safety systems now fully functional
- Multiple layers of protection
- Failsafe defaults to motors disabled
- Recovery timers prevent rapid cycling

---

## [Version 1.0] - June 2022 (Original)

### Initial Release
- Basic motor control (4 motors with PWM and direction)
- CRSF receiver integration (AlfredoCRSF library)
- Manual mode (pedal + shifter)
- Remote mode (ELRS radio control)
- Basic telemetry output
- Lights control

### Known Issues (Fixed in v2.0)
- ❌ PWM never initialized (critical bug - motors wouldn't work)
- ❌ No safety systems implemented
- ❌ No voltage or current monitoring
- ❌ No failsafe for radio signal loss
- ❌ Temperature fault read but ignored
- ❌ No motor ramping (instant acceleration)
- ❌ PARK mode didn't disable motors
- ❌ Used deprecated ESP32 Core 2.x API
- ❌ Included unused ESP32Servo library
- ❌ Minimal documentation and comments

---

## Migration Guide: v1.0 → v2.0

### For Users Upgrading

1. **Update ESP32 Arduino Core to 3.x**
   - Board Manager → ESP32 → Update to latest 3.x version
   - Verify version in Arduino IDE

2. **No Hardware Changes Required**
   - Pin assignments unchanged
   - Existing wiring compatible

3. **Add Sensors (Optional but Recommended)**
   - Voltage divider for battery monitoring (82kΩ + 12kΩ)
   - Current sensor (ACS712 or similar)
   - Connect to pins 35 and 36 respectively

4. **Adjust Safety Constants**
   - Edit `LOW_VOLTAGE_CUTOFF` for your battery type
   - Edit `MAX_CURRENT_LIMIT` for your motors
   - Edit `CURRENT_SENSOR_*` for your sensor type

5. **Test Thoroughly**
   - Start with `DEVELOPMENT_MODE = true`
   - Test motors with no load
   - Test all safety systems
   - Full system test before use

### For Developers

- Replace any `ledcSetup()` calls with `ledcAttach(pin, freq, res)`
- Replace any `ledcAttachPin()` calls (not needed in v3.x)
- Change `ledcWrite(channel, duty)` to `ledcWrite(pin, duty)`
- Use GPIO pin numbers, not channel numbers
- Review new safety functions and integrate into custom logic

---

## Future Versions (Planned)

### [Version 2.1] - Q1 2026 (Planned)
- [ ] Steering motor control implementation
- [ ] Horn functionality via audio system
- [ ] DFPlayer Mini integration for engine sounds
- [ ] Speed limiting via external potentiometer

### [Version 3.0] - Q2 2026 (Planned)
- [ ] WiFi configuration portal
- [ ] Web-based telemetry dashboard
- [ ] Over-the-air (OTA) firmware updates
- [ ] Data logging to SD card
- [ ] Battery percentage calculation
- [ ] Multi-language support

---

**Note**: Version numbers follow semantic versioning (MAJOR.MINOR.PATCH)
- MAJOR: Breaking changes, API changes, incompatible updates
- MINOR: New features, backward-compatible additions
- PATCH: Bug fixes, minor improvements

