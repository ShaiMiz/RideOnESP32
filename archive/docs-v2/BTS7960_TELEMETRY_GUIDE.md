# BTS7960 Current Sensing and Telemetry Guide

## 📊 Complete Guide to Reading BTS7960 Built-in Current Sensors

Your firmware now includes comprehensive support for reading **all current sensing data** from the 4x BTS7960 motor drivers.

---

## 🔌 BTS7960 Current Sense Outputs

### **What is R_IS and L_IS?**

Each BTS7960 motor driver has **two current sense outputs**:
- **R_IS** (Right Current Sense): Outputs voltage proportional to current flowing through RPWM (forward direction)
- **L_IS** (Left Current Sense): Outputs voltage proportional to current flowing through LPWM (reverse direction)

### **Specifications**
| Parameter | Typical Value | Notes |
|-----------|---------------|-------|
| **Sensitivity** | 1mV per 1A | Some modules: 0.5mV/A or 2mV/A - check datasheet |
| **Output Voltage** | 0 to 3.3V | Safe for ESP32 ADC directly |
| **Response Time** | <1µs | Real-time current measurement |
| **Accuracy** | ±5% | Sufficient for monitoring and protection |
| **Max Current** | 43A continuous | Beyond this, BTS7960 thermal limits apply |

---

## 📌 Pin Assignments (All 8 Channels)

Your firmware now reads **8 ADC channels** for complete per-motor current monitoring:

### **Pin Mapping**

| Motor | R_IS Pin (Forward) | L_IS Pin (Reverse) | ESP32 GPIO |
|-------|-------------------|-------------------|------------|
| **Front-Left** | R_IS | L_IS | GPIO 39, GPIO 36 |
| **Front-Right** | R_IS | ~~L_IS~~ | GPIO 34, ~~GPIO 35~~ |
| **Rear-Left** | R_IS | L_IS | GPIO 32, GPIO 33 |
| **Rear-Right** | R_IS | L_IS | GPIO 37, GPIO 38 |

**Note about GPIO 35**: This pin is used for battery voltage monitoring (higher priority). Front-right L_IS current is estimated from R_IS.

**Note about GPIO 37/38**: These pins may not be available on all ESP32 modules (some only expose GPIOs 0-39 partially). If unavailable, rear-right current is estimated from other motors.

### **Code Pin Definitions**
```cpp
// Front-Left Motor
#define FL_R_IS  39  // Forward current sense
#define FL_L_IS  36  // Reverse current sense

// Front-Right Motor
#define FR_R_IS  34  // Forward current sense
// FR_L_IS: GPIO 35 used for battery voltage

// Rear-Left Motor
#define RL_R_IS  32  // Forward current sense
#define RL_L_IS  33  // Reverse current sense

// Rear-Right Motor
#define RR_R_IS  37  // Forward current sense (if available)
#define RR_L_IS  38  // Reverse current sense (if available)
```

---

## 🔌 Physical Wiring

### **Per Motor Driver (Example: Front-Left)**

```
BTS7960 #1 (Front-Left)        ESP32
┌──────────────────┐          ┌──────────────────┐
│                  │          │                  │
│  Control Side:   │          │                  │
│  VCC  ───────────┼──────────┤ 3.3V             │
│  GND  ───────────┼──────────┤ GND              │
│  RPWM ───────────┼──────────┤ GPIO 25          │
│  LPWM ───────────┼──────────┤ GPIO 26          │
│  R_EN ───────────┤          │ (tied to VCC)    │
│  L_EN ───────────┤          │ (tied to VCC)    │
│                  │          │                  │
│ ┌──Telemetry───┐ │          │                  │
│ │ R_IS ─────────┼─┼──────────┤ GPIO 39 (ADC)    │
│ │ L_IS ─────────┼─┼──────────┤ GPIO 36 (ADC)    │
│ └──────────────┘ │          │                  │
│                  │          │                  │
│  Power Side:     │          │                  │
│  B+  ────────────┤ 24V      │                  │
│  B-  ────────────┤ GND      │                  │
│  M+  ────────────┤ Motor+   │                  │
│  M-  ────────────┤ Motor-   │                  │
└──────────────────┘          └──────────────────┘
```

### **Complete Wiring for All 4 Motors**

| BTS7960 | R_IS Output | L_IS Output | Connect To ESP32 |
|---------|-------------|-------------|------------------|
| **FL** | Pin | Pin | GPIO 39, GPIO 36 |
| **FR** | Pin | Pin | GPIO 34, (GPIO 35 used for voltage) |
| **RL** | Pin | Pin | GPIO 32, GPIO 33 |
| **RR** | Pin | Pin | GPIO 37, GPIO 38 |

**⚠️ Important**: All grounds must be common (ESP32 GND, BTS7960 GND, battery GND).

---

## 📈 How Current Sensing Works

### **Voltage-to-Current Conversion**

```
Current (A) = V_sense / Sensitivity

Example:
- V_sense = 0.010V (10mV measured at R_IS pin)
- Sensitivity = 0.001 V/A (1mV per amp)
- Current = 0.010 / 0.001 = 10A
```

### **In Code**
```cpp
// Read ADC (0-4095 for 12-bit)
int adc_value = analogRead(FL_R_IS);

// Convert to voltage (0-3.3V)
float voltage = (adc_value / 4095.0) * 3.3;

// Convert to current (assuming 1mV/A sensitivity)
float current = voltage / 0.001; // or voltage * 1000.0
```

### **Adjusting Sensitivity**

If your BTS7960 module has different sensitivity:

```cpp
// In code (around line 90):
const float BTS7960_CURRENT_SENSITIVITY = 0.001; // 1mV/A (typical)

// For 0.5mV/A sensitivity:
const float BTS7960_CURRENT_SENSITIVITY = 0.0005;

// For 2mV/A sensitivity:
const float BTS7960_CURRENT_SENSITIVITY = 0.002;
```

**How to determine sensitivity:**
1. Connect a known current load (e.g., 10A)
2. Measure voltage at R_IS pin with multimeter
3. Calculate: Sensitivity = V_measured / I_load
4. Example: 10mV at 10A → Sensitivity = 0.001 V/A

---

## 📊 What You Get From Telemetry

### **Data Structure**

Each motor has a `MotorTelemetry` struct:
```cpp
struct MotorTelemetry {
  float forwardCurrent;  // Current when RPWM active (from R_IS)
  float reverseCurrent;  // Current when LPWM active (from L_IS)
  float totalCurrent;    // Combined current
};

// Global variables:
MotorTelemetry frontLeft;
MotorTelemetry frontRight;
MotorTelemetry rearLeft;
MotorTelemetry rearRight;
```

### **Telemetry Output Example**

```
[1000 ms] Status: MANUAL | Lights: ON | Shifter: DRIVE | Direction: FWD
[1000 ms] Power: Battery=24.35V | Total I=28.45A | Motors=ON | PWM=180/180
[1000 ms] Motor I: FL=7.12A | FR=7.08A | RL=7.15A | RR=7.10A
```

**What This Tells You:**
- Battery voltage: 24.35V (healthy)
- Total current: 28.45A (4 motors × ~7A each)
- Per-motor current: All around 7A (balanced - good!)
- Motors all drawing similar current → no mechanical issues

### **Detecting Problems**

**Example 1: Motor Stall**
```
[5000 ms] Motor I: FL=5.2A | FR=5.1A | RL=15.8A | RR=5.3A
[5000 ms] WARNING: Motor RL current imbalance detected! (8.5A deviation)
[5000 ms]   Possible causes: Motor stall, mechanical binding, damaged motor
```
**Diagnosis**: Rear-left motor drawing 3x more current → check for jammed wheel or damaged motor.

**Example 2: Disconnected Motor**
```
[3000 ms] Motor I: FL=8.2A | FR=0.0A | RL=8.5A | RR=8.1A
[3000 ms] WARNING: Motor FR current imbalance detected! (6.3A deviation)
```
**Diagnosis**: Front-right motor drawing no current → check wiring or motor connection.

**Example 3: Overload**
```
[2000 ms] Power: Battery=23.8V | Total I=42.3A | Motors=ON
[2000 ms] Motor I: FL=10.5A | FR=10.8A | RL=10.2A | RR=10.8A
[2000 ms] WARNING: High current draw (42.3A)
```
**Diagnosis**: All motors overloaded → check for heavy load, uphill climb, or mechanical resistance.

---

## 🔍 Advanced Features

### **1. Direction-Specific Current Monitoring**

Uncomment this code block in `outputTelemetry()` for even more detail:

```cpp
if (motorDirection) {
  // Forward direction - show R_IS readings
  String fwdCurrents = "Forward I: ";
  fwdCurrents += "FL=" + String(frontLeft.forwardCurrent, 2) + "A";
  fwdCurrents += " | FR=" + String(frontRight.forwardCurrent, 2) + "A";
  fwdCurrents += " | RL=" + String(rearLeft.forwardCurrent, 2) + "A";
  fwdCurrents += " | RR=" + String(rearRight.forwardCurrent, 2) + "A";
  logMessage(fwdCurrents);
}
```

**Output:**
```
[1000 ms] Forward I: FL=7.12A | FR=7.08A | RL=7.15A | RR=7.10A
```

### **2. Current Imbalance Detection**

The firmware automatically detects if one motor draws significantly more current than others:

```cpp
// In outputTelemetry():
// Warns if one motor differs by >5A from average
if (maxDeviation > 5.0 && motorCurrent > 5.0) {
  logMessage("WARNING: Motor " + problematicMotor + " current imbalance!");
}
```

**Use Cases:**
- Detect motor damage before complete failure
- Identify mechanical binding or wheel obstruction
- Monitor for uneven load distribution
- Preventive maintenance alerts

### **3. Integration with Safety System**

Current readings are integrated into safety checks:

```cpp
// In checkSafetyConditions():
if (motorCurrent > MAX_CURRENT_LIMIT) {
  safetyOK = false;
  // Motors disabled automatically
}
```

**Safety Thresholds:**
```cpp
const float MAX_CURRENT_LIMIT = 40.0;     // Emergency shutdown at 40A
const float HIGH_CURRENT_WARNING = 35.0;  // Warning at 35A
```

Adjust these based on your motors and battery capacity.

---

## 🧪 Testing and Calibration

### **Step 1: Verify Wiring**

1. Connect all R_IS and L_IS pins to ESP32 ADC pins
2. Upload firmware
3. Open Serial Monitor (115200 baud)
4. Check for initialization message:
   ```
   [150 ms] BTS7960 current sense inputs configured (8 channels)
   ```

### **Step 2: Zero Current Test**

1. Power on ESP32 and BTS7960 modules
2. **Do NOT connect motors yet**
3. Set `DEVELOPMENT_MODE = false`
4. Observe telemetry output:
   ```
   [1000 ms] Motor I: FL=0.00A | FR=0.00A | RL=0.00A | RR=0.00A
   ```

**Expected**: All currents near 0A (±0.1A due to noise)

**If readings are high (>1A):**
- Check wiring (R_IS/L_IS connected correctly?)
- Verify common ground
- Check BTS7960 VCC is stable 3.3V or 5V

### **Step 3: Motor Load Test**

1. Connect motors
2. Lift vehicle (wheels off ground)
3. Set to manual mode, shifter to DRIVE
4. Press pedal slowly
5. Observe current ramp up:
   ```
   [5000 ms] Motor I: FL=2.5A | FR=2.4A | RL=2.6A | RR=2.5A (no load)
   [6000 ms] Motor I: FL=5.1A | FR=5.0A | RL=5.2A | RR=5.1A (light throttle)
   [7000 ms] Motor I: FL=8.2A | FR=8.1A | RL=8.4A | RR=8.2A (full throttle)
   ```

**Expected**: All motors draw similar current (±1A variation is normal)

### **Step 4: Full Load Test**

1. Place vehicle on ground
2. Add weight (simulate rider)
3. Drive forward
4. Observe current under load:
   ```
   [10000 ms] Motor I: FL=12.5A | FR=12.3A | RL=12.8A | RR=12.4A
   ```

**Expected**: Higher current under load, all motors still balanced

**If one motor draws much more:**
- Check for mechanical binding in that wheel
- Verify motor connections
- Inspect for damaged motor

---

## 📉 Calibration Guide

### **If Your BTS7960 Has Different Sensitivity**

Most modules use **1mV/A**, but some vary:

**Test Method:**
1. Connect a **known resistive load** (e.g., power resistor)
2. Apply known voltage to create known current
   - Example: 24V across 2.4Ω resistor = 10A
3. Measure voltage at R_IS pin with **multimeter**
4. Calculate sensitivity:
   ```
   Sensitivity = V_measured / Current
   Example: 10mV / 10A = 0.001 V/A (1mV/A)
   ```

**Update in code:**
```cpp
const float BTS7960_CURRENT_SENSITIVITY = 0.001; // Your measured value
```

---

## 🛠️ Troubleshooting

### **Problem: All Current Readings Show 0A**

**Possible Causes:**
1. R_IS/L_IS pins not connected to ESP32
2. Wrong GPIO pin numbers in code
3. BTS7960 modules not powered
4. ADC pins not configured correctly

**Solution:**
- Verify physical wiring with multimeter
- Measure voltage at R_IS pin when motor running (should be >0V)
- Check `pinMode()` calls in setup() include all current sense pins

### **Problem: Current Readings Very High (>50A) or Unrealistic**

**Possible Causes:**
1. Wrong sensitivity constant in code
2. ADC reference voltage incorrect
3. Noise on ADC lines

**Solution:**
- Verify `BTS7960_CURRENT_SENSITIVITY` matches your module
- Add small capacitor (0.1µF) between R_IS/L_IS and GND (near ESP32)
- Check for loose connections

### **Problem: One Motor Shows No Current**

**Possible Causes:**
1. R_IS/L_IS pin not connected for that motor
2. BTS7960 module for that motor not powered
3. Motor or driver damaged

**Solution:**
- Check wiring for that specific motor's current sense pins
- Verify BTS7960 VCC is present
- Test motor with different driver to isolate issue

### **Problem: GPIO 37/38 Not Available**

**Symptoms:**
- Compiler error or rear-right motor shows estimated current

**Solution:**
- This is normal for some ESP32 modules (e.g., ESP32-WROOM-32 with limited pins)
- Firmware automatically falls back to estimation
- Alternative: Use external ADC multiplexer (CD4051) to share one ADC pin

---

## 📊 Data Logging and Analysis

### **Logging to SD Card (Future Enhancement)**

Add SD card module to log telemetry:

```cpp
#include <SD.h>

void logToSD() {
  File dataFile = SD.open("telemetry.csv", FILE_APPEND);
  if (dataFile) {
    dataFile.print(millis());
    dataFile.print(",");
    dataFile.print(batteryVoltage);
    dataFile.print(",");
    dataFile.print(frontLeft.totalCurrent);
    dataFile.print(",");
    dataFile.print(frontRight.totalCurrent);
    // ... etc
    dataFile.println();
    dataFile.close();
  }
}
```

### **Plotting Current vs Time**

Use Serial Plotter (Arduino IDE):
1. Tools → Serial Plotter
2. Modify `outputTelemetry()` to output CSV format:
   ```cpp
   Serial.print(frontLeft.totalCurrent);
   Serial.print(",");
   Serial.print(frontRight.totalCurrent);
   Serial.print(",");
   Serial.print(rearLeft.totalCurrent);
   Serial.print(",");
   Serial.println(rearRight.totalCurrent);
   ```
3. Watch real-time current graph

---

## 🎯 Summary

### **What You Have Now**

✅ **8-channel current monitoring** (R_IS + L_IS for 4 motors)  
✅ **Per-motor current display** in telemetry  
✅ **Direction-specific current** (forward vs reverse)  
✅ **Automatic imbalance detection** (identifies problematic motors)  
✅ **Integration with safety system** (overcurrent protection)  
✅ **Real-time diagnostics** (1-second telemetry updates)  

### **Benefits**

🔍 **Diagnostics**: Identify motor/mechanical issues before failure  
🛡️ **Safety**: Detect stalls, overloads, and electrical faults  
⚡ **Efficiency**: Monitor power consumption per motor  
🔧 **Maintenance**: Predictive alerts for service needs  

### **Next Steps**

1. Wire all R_IS and L_IS pins to ESP32 ADC pins
2. Upload firmware v2.1
3. Test with motors (follow testing procedure above)
4. Calibrate sensitivity if needed
5. Monitor telemetry during normal operation
6. Enjoy comprehensive motor monitoring! 🚗💨

---

**Questions?** Review the troubleshooting section or check Serial Monitor output for diagnostic messages.

