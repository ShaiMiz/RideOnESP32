# Hardware-Specific Updates - Version 2.1

## 📋 Summary of Changes Based on Your Actual Hardware

This document summarizes the updates made after reviewing your [Component_Descriptions_and_Purpose.pdf](file:///c%3A/Users/Shai%20Mizrahi/Documents/RideOnESP32/Component_Descriptions_and_Purpose.pdf) and [ESP32_ELRS_Receiver_Connection_Guide.pdf](file:///c%3A/Users/Shai%20Mizrahi/Documents/RideOnESP32/ESP32_ELRS_Receiver_Connection_Guide.pdf).

---

## ✅ What Was CORRECT (No Changes Needed)

### 1. **CRSF Protocol** ✅
- Your **Jumper T-Lite V2 with ELRS** + **ELRS Nano 2.4GHz receiver** uses **CRSF protocol**
- The `AlfredoCRSF` library is **correct** for ExpressLRS systems
- CRSF is the standard protocol for ELRS (not SBUS)
- Serial2 at 420000 baud is correct

### 2. **ESP32 Power** ✅
- Your system uses **3.3V from LM2596** for ESP32 (correct!)
- This prevents overheating and is the proper voltage
- No changes needed to power configuration

---

## 🔄 What Was UPDATED

### 1. **24V Battery System** (was 12V)

**Changes Made:**
- Updated voltage divider ratio from **7.833** to **9.333**
  - Old: 82kΩ + 12kΩ (for 12V system)
  - **New: 100kΩ + 12kΩ (for 24V system)**
- Updated voltage thresholds:
  - `LOW_VOLTAGE_CUTOFF`: 10.5V → **19.0V**
  - `LOW_VOLTAGE_WARNING`: 11.0V → **21.0V**
- Updated current limits:
  - `MAX_CURRENT_LIMIT`: 15.0A → **40.0A** (4 motors × 200W / 24V = ~33A)
  - `HIGH_CURRENT_WARNING`: 12.0A → **35.0A**
- Updated current sensor recommendation:
  - ACS712-05B → **ACS712-30B** (for higher current capacity)
  - Sensitivity: 0.185 V/A → **0.066 V/A**

**Why This Matters:**
- 24V system has different voltage ranges
- Higher power motors draw more current
- Voltage divider must be scaled to keep ADC input <3.3V

### 2. **BTS7960 Motor Drivers** (was generic H-bridge)

**Changes Made:**
- Updated pin definitions:
  - Old: `FL_PWM` + `FL_DIR` (single PWM + direction pin)
  - **New: `FL_RPWM` + `FL_LPWM`** (dual PWM for forward/reverse)
- Added 8 PWM channels (was 4):
  - 4x RPWM pins (forward direction)
  - 4x LPWM pins (reverse direction)
- Updated control logic:
  - Forward: RPWM = speed, LPWM = 0
  - Reverse: RPWM = 0, LPWM = speed
  - Brake: RPWM = 0, LPWM = 0
- Updated `setMotorDirection()` function to store direction flag
- Updated `updateMotorRamping()` to write to correct PWM pins based on direction
- Updated `emergencyStop()` to set both RPWM and LPWM to 0

**Why This Matters:**
- BTS7960 uses different control method than generic H-bridges
- Requires twice as many PWM channels
- More efficient control (no separate direction pins)

### 3. **Motor Specifications**

**Added Documentation:**
- Motors: **24V 22k RPM 200W** (4 units)
- Driver specs: **43A continuous, 100A peak** per BTS7960
- Total power: **800W** (4 × 200W)
- Total current: **~33A** at full load

### 4. **Protection System**

**Added Documentation:**
- **XH-M609 battery discharge protection** (cuts off at ~20V)
- **30A resettable circuit breaker**
- **LM2596 buck converters** (3.3V for ESP32, 12V for accessories)

---

## 📊 Pin Assignment Changes

### Old Pin Definitions (Generic H-Bridge)
```cpp
#define FL_PWM  25
#define FL_DIR  26
#define FR_PWM  27
#define FR_DIR  14
// etc...
```

### New Pin Definitions (BTS7960)
```cpp
#define FL_RPWM  25  // Forward PWM
#define FL_LPWM  26  // Reverse PWM
#define FR_RPWM  27
#define FR_LPWM  14
// etc...
```

**Total PWM Channels:**
- Old: 4 channels (one per motor)
- **New: 8 channels** (two per motor - RPWM + LPWM)

---

## 💡 ELRS Receiver Power: 5V vs 3.3V

### Your Question: "Why use 5V for ESP32/ELRS and not 3.3V?"

**Answer:**

1. **ESP32 Power**: Your system correctly uses **3.3V** (from LM2596 #1)
   - This is the proper voltage for ESP32
   - Prevents overheating
   - No changes needed

2. **ELRS Receiver Power**: Can use either, but **5V is recommended**

| Voltage | Pros | Cons |
|---------|------|------|
| **5V** (Recommended) | ✅ Better signal strength<br>✅ Longer range<br>✅ More reliable in noisy environments<br>✅ Better immunity to motor EMI | ❌ Need 5V regulator from 12V line |
| **3.3V** (Simpler) | ✅ Share ESP32 power supply<br>✅ Simpler wiring<br>✅ Works for most receivers | ⚠️ May reduce range<br>⚠️ Less reliable near motors |

**Recommendation for Your 24V System:**
- Use **5V for ELRS receiver** (add small 5V regulator from 12V LM2596 output)
- Your 4x 200W motors create significant electrical noise (EMI)
- 5V gives better noise immunity and range
- CRSF signal pins are 3.3V compatible, so direct connection to ESP32 is safe

**How to Add 5V:**
```
12V LM2596 → [LM7805 or AMS1117-5.0] → ELRS VCC (5V)
                        ↓
                       GND
```

---

## 📁 New Documentation Files

### 1. **BTS7960_WIRING_GUIDE.md** (NEW)
Comprehensive wiring guide specific to your hardware:
- Complete BTS7960 pinout and connections
- 24V power distribution diagram
- Voltage divider calculations for 24V
- Current sensor options (ACS712-30B)
- ELRS receiver wiring with 5V option
- Complete pin assignment table
- Testing procedures
- Troubleshooting specific to BTS7960

### 2. **Updated README.md**
- Added hardware specifications section
- Updated component list for 24V system
- Updated BTS7960 motor drivers
- Updated voltage/current thresholds

### 3. **Updated sketch_jun22b.ino**
- Version 2.1
- BTS7960-specific control logic
- 24V battery thresholds
- 8 PWM channels (RPWM + LPWM)
- Updated comments and documentation

---

## 🔧 Configuration Changes You Need to Make

### 1. **Voltage Divider** (CRITICAL!)

**Build this circuit:**
```
24V Battery +
    │
   [100kΩ] ←─ R1 (NOT 82kΩ!)
    │
    ├──────────→ ESP32 GPIO 35
    │
   [12kΩ]  ←─ R2
    │
   GND
```

**⚠️ WARNING**: Using 82kΩ + 12kΩ (from 12V system) with 24V battery would give **3.06V** at GPIO 35, which is too close to the 3.3V limit and could damage ESP32!

**With 100kΩ + 12kΩ:**
- At 24V: 2.57V ✅ (safe)
- At 27V: 2.89V ✅ (safe)
- At 30V: 3.21V ✅ (safe, <3.3V)

### 2. **Current Sensor** (if using external sensor)

**Use ACS712-30B** (NOT ACS712-05B):
- 05B: ±5A range (too small for your system)
- 20B: ±20A range (marginal)
- **30B: ±30A range** (correct for 4x 200W motors)

**Add voltage divider on ACS712 output** (outputs 0-5V, ESP32 max is 3.3V):
```
ACS712 OUT
    │
   [10kΩ]
    │
    ├──────→ ESP32 GPIO 36
    │
   [22kΩ]
    │
   GND
```

### 3. **Code Constants** (already updated in v2.1)

```cpp
// Voltage divider for 24V system
const float VOLTAGE_DIVIDER_RATIO = 9.333;  // 100k + 12k

// 24V battery thresholds
const float LOW_VOLTAGE_CUTOFF = 19.0;  // Volts
const float LOW_VOLTAGE_WARNING = 21.0; // Volts

// Current limits for 800W total (4x 200W motors)
const float MAX_CURRENT_LIMIT = 40.0;     // Amps
const float HIGH_CURRENT_WARNING = 35.0;  // Amps

// Current sensor for ACS712-30B
const float CURRENT_SENSITIVITY = 0.066;  // V/A (66mV per amp)
```

---

## ✅ Verification Checklist

Before powering on:

- [ ] **Voltage divider uses 100kΩ + 12kΩ** (NOT 82kΩ + 12kΩ)
- [ ] **Measured voltage at GPIO 35 < 3.3V** with battery at max voltage
- [ ] **LM2596 #1 set to exactly 3.3V** (measure with multimeter)
- [ ] **LM2596 #2 set to exactly 12V** (measure with multimeter)
- [ ] **All 8 PWM pins connected** (4x RPWM + 4x LPWM)
- [ ] **BTS7960 B+ connected to 24V** (from XH-M609 output)
- [ ] **All grounds common** (ESP32, battery, BTS7960, ELRS)
- [ ] **ELRS receiver powered** (3.3V or 5V - 5V recommended)
- [ ] **Current sensor has voltage divider** (if using ACS712)

---

## 📚 Reference Documents

1. **BTS7960_WIRING_GUIDE.md** - Complete wiring for your specific hardware
2. **README.md** - Software features and configuration
3. **CHANGELOG.md** - Version history and changes
4. **sketch_jun22b.ino** - Updated firmware (v2.1)

---

## 🎯 Summary

**What You Have:**
- 24V battery system (original Can-Am X3)
- 4x BTS7960 motor drivers
- 4x 24V 200W motors
- Jumper T-Lite V2 + ELRS Nano 2.4GHz
- XH-M609 protection + 30A breaker
- LM2596 converters (3.3V + 12V)

**What Changed in Firmware:**
- ✅ Updated for 24V voltage thresholds
- ✅ Updated for BTS7960 dual-PWM control
- ✅ Updated for higher current capacity
- ✅ Added comprehensive documentation
- ✅ CRSF protocol was already correct!

**What You Need to Build:**
- ⚠️ **100kΩ + 12kΩ voltage divider** (for 24V monitoring)
- ⚠️ **Voltage divider for ACS712** (if using external current sensor)
- 💡 **5V regulator for ELRS** (optional but recommended)

---

**Ready to wire and test!** Follow the **BTS7960_WIRING_GUIDE.md** for complete step-by-step instructions.

