# ESP32 GPIO Pin Allocation - Optimized for Can-Am X3

## ✅ Critical Functions (Cannot Change)

| GPIO | Function | Type | Notes |
|------|----------|------|-------|
| **5** | SHIFTER_R | Input | Manual reverse switch |
| **12** | RL_RPWM | PWM | Motor control (⚠️ boot pin) |
| **13** | RL_LPWM | PWM | Motor control |
| **14** | FR_LPWM | PWM | Motor control |
| **16** | UART2 RX | UART | ELRS receiver CRSF |
| **17** | UART2 TX | UART | ELRS receiver CRSF |
| **19** | TEMP_FAULT | Input | Combined thermal monitoring |
| **21** | MOTOR_ENABLE | Output | BTS7960 enable (all 4) |
| **22** | LIGHT_PIN | Output | Lights control |
| **25** | FL_RPWM | PWM | Motor control |
| **26** | FL_LPWM | PWM | Motor control |
| **27** | FR_RPWM | PWM | Motor control |
| **32** | RR_RPWM | PWM | Motor control |
| **33** | RR_LPWM | PWM | Motor control |
| **35** | VOLTAGE_PIN | ADC | Battery voltage (priority) |
| **36** | FL_L_IS | ADC | Front-left reverse current |

## 🔄 Shared Pins (Mode-Dependent)

| GPIO | Function 1 | Function 2 | Usage |
|------|------------|------------|-------|
| **34** | PEDAL_PIN (MANUAL) | FR_R_IS (REMOTE) | Mode-switched |
| **39** | SHIFTER_D (MANUAL) | FL_R_IS (REMOTE) | Mode-switched |

## ⚠️ Optional Functions (Can Be Disabled)

| GPIO | Function | Can Free? | Notes |
|------|----------|-----------|-------|
| **2** | STEERING_DIR | ✅ YES | If no power steering |
| **4** | AUDIO_VOLDOWN | ✅ YES | Not implemented |
| **15** | STEERING_PWM | ✅ YES | If no power steering |
| **18** | AUDIO_VOLUP | ✅ YES | Not implemented |
| **23** | AUDIO_PLAY | ✅ YES | Not implemented |

## 🚫 Not Available / Reserved

| GPIO | Reason |
|------|--------|
| **0** | Boot mode / flash programming |
| **1** | USB Serial TX (console) |
| **3** | USB Serial RX (console) |
| **6-11** | Flash memory (cannot use) |
| **37-38** | Not exposed on ESP32-WROOM-32U |

## 📊 Summary

- **Total ESP32 GPIOs:** 39 (GPIO 0-39)
- **Unusable:** 10 (flash, console, not exposed)
- **Available:** 29 GPIOs
- **Currently used:** 26 functions
- **Free (audio pins):** 3 GPIOs (4, 18, 23)
- **Can free (steering):** 2 GPIOs (2, 15)

## 🎯 Recommendations

### For Thermal Monitoring:
- **Option 1:** Combined monitoring (GPIO 19 with diode OR) ✅ **RECOMMENDED**
- **Option 2:** Temperature sensors on GPIO 15 (disable steering)

### For Current Sensing:
- Currently limited due to pin conflicts
- Only FL and FR current sense available in REMOTE mode
- Consider external ADC (ADS1115) if more channels needed

### Priority Order:
1. Motor control (8 PWM pins) - CRITICAL
2. ELRS receiver (2 UART pins) - CRITICAL
3. Battery voltage monitoring - CRITICAL
4. Thermal protection (1 pin) - CRITICAL
5. Manual controls (pedal + shifter) - HIGH
6. Lights control - MEDIUM
7. Current sensing - MEDIUM (limited by available ADC pins)
8. Steering - LOW (not implemented)
9. Audio - LOW (not implemented)


