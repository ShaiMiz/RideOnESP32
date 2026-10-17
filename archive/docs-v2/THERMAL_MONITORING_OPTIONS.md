# Thermal Monitoring Options for BTS7960 Drivers

## ❌ Problem: Insufficient GPIO Pins for Individual Monitoring

### GPIO Availability Analysis:
- **Need for individual monitoring:** 4 GPIOs (one per BTS7960 driver)
- **Available free pins:** 3 GPIOs (audio pins 4, 18, 23)
- **Result:** SHORT by 1 GPIO pin

---

## ✅ Solution 1: Combined Thermal Monitoring (RECOMMENDED)

### Implementation:
Wire all 4 BTS7960 FAULT outputs to **GPIO 19** using diode OR logic.

### Hardware Required:
- 4x **1N4148 diodes** (~$0.50 total)
- 1x **10kΩ resistor** (pull-up)

### Wiring Diagram:
```
BTS7960 FL FAULT ──┤>|─1N4148──┐
                                │
BTS7960 FR FAULT ──┤>|─1N4148──┤
                                ├──→ ESP32 GPIO 19 ──[10kΩ]─→ 3.3V
BTS7960 RL FAULT ──┤>|─1N4148──┤
                                │
BTS7960 RR FAULT ──┤>|─1N4148──┘

All BTS7960 GND ────────────────→ Common GND
```

### How It Works:
1. BTS7960 FAULT pins are **active LOW** (LOW = overheating)
2. Diodes create **wired-OR logic**
3. If **ANY** driver overheats → GPIO 19 goes LOW
4. Pull-up resistor keeps GPIO 19 HIGH when all drivers OK
5. ESP32 monitors GPIO 19: LOW = emergency stop

### Pros:
✅ Uses only **1 GPIO pin**  
✅ Already implemented in firmware (default mode)  
✅ Protects all 4 drivers with single safety check  
✅ Simple and reliable  
✅ No code changes needed  
✅ Diodes prevent drivers from interfering with each other  

### Cons:
❌ Cannot identify **which specific driver** is overheating  
❌ Must shutdown all motors even if only one driver is hot  

### Firmware Implementation:
Already active by default:
```cpp
// Line ~100 in sketch_jun22b.ino
#define TEMP_FAULT_PIN   19  // Combined fault input
```

No action required - this is the default configuration.

---

## ✅ Solution 2: Temperature Sensors (BEST DIAGNOSTICS)

### Implementation:
Add **4x DS18B20** digital temperature sensors to BTS7960 heat sinks.

### Hardware Required:
- 4x **DS18B20 waterproof sensors** (~$8-12 total)
- 1x **4.7kΩ resistor** (pull-up for 1-Wire bus)
- Thermal adhesive tape or paste

### Wiring Diagram:
```
DS18B20 #1 (FL) ──┐
DS18B20 #2 (FR) ──┤
DS18B20 #3 (RL) ──┼── DATA → ESP32 GPIO 15 ──[4.7kΩ]─→ 3.3V
DS18B20 #4 (RR) ──┘

All VCC ───────────→ 3.3V
All GND ───────────→ Common GND
```

### GPIO Required:
- **GPIO 15** (currently assigned to STEERING_PWM)
- Must disable steering control if not using power steering

### How It Works:
1. DS18B20 sensors mounted on each BTS7960 heat sink
2. All 4 sensors share **1-Wire bus** (only 1 GPIO needed!)
3. Each sensor has unique address (auto-detected)
4. ESP32 reads actual temperature in °C from each driver
5. Can set **warning thresholds** before critical fault occurs

### Pros:
✅ Uses only **1 GPIO pin** (1-Wire bus)  
✅ **Identifies which driver** is getting hot  
✅ **Actual temperature readings** (not just ON/OFF fault)  
✅ **Proactive warnings** (e.g., warning at 70°C, critical at 85°C)  
✅ Better diagnostics for troubleshooting  
✅ Can log temperature trends over time  
✅ BTS7960 hardware fault still works as backup  

### Cons:
❌ Requires additional hardware (~$10)  
❌ Requires firmware modification (see code below)  
❌ Requires disabling steering control (GPIO 15)  

### Firmware Modification:

#### 1. Add Library to platformio.ini:
```ini
lib_deps = 
    https://github.com/AlfredoSystems/AlfredoCRSF.git
    milesburton/DallasTemperature@^3.11.0
    paulstoffregen/OneWire@^2.3.7
```

#### 2. Add to sketch_jun22b.ino:
```cpp
// After includes section:
#include <OneWire.h>
#include <DallasTemperature.h>

// Pin definitions section:
#define TEMP_SENSOR_PIN 15  // 1-Wire bus for DS18B20 sensors

// After constants section:
const float TEMP_WARNING = 70.0;   // °C - Start warning
const float TEMP_CRITICAL = 85.0;  // °C - Emergency shutdown
const float TEMP_SAFE = 60.0;      // °C - Safe operating temp

// Global variables section:
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensors(&oneWire);
float driverTemp[4] = {0.0, 0.0, 0.0, 0.0};  // FL, FR, RL, RR

// In setup():
void setup() {
  // ... existing setup code ...
  
  // Initialize temperature sensors
  tempSensors.begin();
  int sensorCount = tempSensors.getDeviceCount();
  if (sensorCount == 4) {
    logMessage("DS18B20: 4 temperature sensors detected");
  } else {
    logMessage("WARNING: Expected 4 sensors, found " + String(sensorCount));
  }
}

// New function:
void readDriverTemperatures() {
  tempSensors.requestTemperatures();
  
  for (int i = 0; i < 4; i++) {
    driverTemp[i] = tempSensors.getTempCByIndex(i);
    
    // Critical temperature shutdown
    if (driverTemp[i] > TEMP_CRITICAL) {
      motorsEnabled = false;
      emergencyStop();
      logMessage("CRITICAL: Driver " + String(i) + " overheating: " + 
                 String(driverTemp[i], 1) + "°C");
    }
    // Warning threshold
    else if (driverTemp[i] > TEMP_WARNING) {
      logMessage("WARNING: Driver " + String(i) + " hot: " + 
                 String(driverTemp[i], 1) + "°C");
    }
  }
}

// In loop(), add after readTelemetry():
void loop() {
  // ... existing code ...
  
  readTelemetry();
  readDriverTemperatures();  // ADD THIS LINE
  
  // ... rest of loop ...
}

// In outputTelemetry(), add:
void outputTelemetry() {
  // ... existing telemetry ...
  
  // Temperature display
  String temps = "Temps: ";
  temps += "FL=" + String(driverTemp[0], 1) + "°C ";
  temps += "FR=" + String(driverTemp[1], 1) + "°C ";
  temps += "RL=" + String(driverTemp[2], 1) + "°C ";
  temps += "RR=" + String(driverTemp[3], 1) + "°C";
  logMessage(temps);
}
```

#### 3. Expected Serial Output:
```
[1000 ms] Status: REMOTE | Lights: ON | Direction: FWD
[1000 ms] Power: Battery=24.3V | Total I=28.4A | Motors=ON
[1000 ms] Motor I: FL=7.1A | FR=7.0A | RL=0.0A | RR=0.0A
[1000 ms] Temps: FL=45.2°C FR=44.8°C RL=46.1°C RR=45.5°C
```

---

## ⚠️ Solution 3: Individual Fault Monitoring (NOT RECOMMENDED)

### Why NOT Recommended:
- Requires **4 GPIO pins** but only **3 are available**
- Must sacrifice additional functionality:
  - **Option A:** Disable steering (frees GPIO 2, 15)
  - **Option B:** Disable lights (frees GPIO 22)
  - **Option C:** Disable shifter reverse (frees GPIO 5)

### If You Choose This Option:

#### Disable Steering Control:
In sketch_jun22b.ino, comment out:
```cpp
// #define STEERING_PWM_PIN 15  // Disabled for thermal monitoring
// #define STEERING_DIR_PIN 2   // Disabled for thermal monitoring
```

Then uncomment:
```cpp
#define TEMP_FAULT_INDIVIDUAL  // Enable individual monitoring
```

This assigns:
- GPIO 19 → FL fault
- GPIO 23 → FR fault
- GPIO 18 → RL fault
- GPIO 2 → RR fault (repurposed from steering)

### Wiring (No Diodes Needed):
```
BTS7960 FL FAULT → ESP32 GPIO 19 (INPUT_PULLUP)
BTS7960 FR FAULT → ESP32 GPIO 23 (INPUT_PULLUP)
BTS7960 RL FAULT → ESP32 GPIO 18 (INPUT_PULLUP)
BTS7960 RR FAULT → ESP32 GPIO 2  (INPUT_PULLUP)

All BTS7960 GND → Common GND
```

### Pros:
✅ Identifies which specific driver is overheating  
✅ Can potentially disable only the hot motor (future enhancement)  

### Cons:
❌ Requires 4 GPIO pins (only 3 available without sacrifices)  
❌ Loses steering control functionality  
❌ Still only digital ON/OFF (not actual temperature)  
❌ More complex wiring  
❌ Temperature sensors are a better solution for same GPIO count  

---

## 📊 Comparison Table

| Feature | Combined (Default) | Temperature Sensors | Individual Faults |
|---------|-------------------|---------------------|-------------------|
| **GPIO pins** | 1 (GPIO 19) | 1 (GPIO 15) | 4 (19,23,18,2) |
| **Hardware cost** | ~$0.50 | ~$10 | $0 |
| **Wiring** | Medium (diodes) | Simple | Simple |
| **Identifies driver** | ❌ No | ✅ Yes | ✅ Yes |
| **Actual temperature** | ❌ No | ✅ Yes (°C) | ❌ No (ON/OFF) |
| **Proactive warnings** | ❌ No | ✅ Yes | ❌ No |
| **Requires sacrifices** | ❌ No | ⚠️ Steering | ⚠️ Steering |
| **Firmware changes** | ❌ None | ✅ Required | ⚠️ Minor |
| **Best for** | Basic safety | Diagnostics | Not recommended |

---

## 🎯 Final Recommendation

### For Most Users:
**Use Combined Monitoring (Default)** - GPIO 19 with diode OR logic
- Adequate thermal protection
- Uses minimal GPIOs
- Already implemented
- Simple and reliable

### For Advanced Diagnostics:
**Add DS18B20 Temperature Sensors** - GPIO 15 (1-Wire)
- Best compromise between features and GPIO usage
- Proactive monitoring
- Individual driver identification
- Actual temperature readings
- Worth the ~$10 investment if you want detailed monitoring

### Avoid:
**Individual Fault Monitoring** - Requires too many GPIOs
- Same GPIO count as temperature sensors
- Less useful (digital ON/OFF vs actual temperature)
- Requires disabling other features

---

## 🛠️ Installation Guide: Combined Monitoring (Recommended)

### Parts List:
- 4x 1N4148 small signal diode
- 1x 10kΩ resistor (1/4W)
- Wire (22 AWG recommended)
- Heat shrink tubing (optional)

### Step-by-Step:

1. **Identify BTS7960 FAULT pins** on all 4 modules
   - May be labeled: FAULT, ALARM, TSD, or similar
   - Check your BTS7960 module datasheet
   - ⚠️ Not all modules expose this pin!

2. **Build diode OR circuit on breadboard first** (testing)
   ```
   BTS7960 #1 FAULT → Diode anode
   BTS7960 #2 FAULT → Diode anode
   BTS7960 #3 FAULT → Diode anode
   BTS7960 #4 FAULT → Diode anode
   
   All diode cathodes → Common point → ESP32 GPIO 19
   10kΩ resistor: Common point → 3.3V
   ```

3. **Test each driver individually**
   - Short each FAULT pin to GND
   - Verify ESP32 detects fault (motors stop)
   - Check Serial Monitor for fault message

4. **Permanent installation**
   - Solder connections or use terminal blocks
   - Heat shrink all connections
   - Mount diodes near ESP32
   - Keep wires away from motor PWM lines (EMI)

5. **Final test**
   - Run motors at high load
   - Verify fault detection works
   - Monitor Serial output

### Troubleshooting:
- **Motors don't stop when FAULT shorted:** Check pull-up resistor, verify GPIO 19 wiring
- **False faults:** Check for loose connections, EMI from motor wires
- **No fault detection:** Verify BTS7960 modules have FAULT pin, check diode orientation

---

## 📚 Additional Resources

- See `BTS7960_WIRING_GUIDE.md` for complete system wiring
- See `GPIO_PIN_ALLOCATION.md` for detailed pin usage
- See `BTS7960_TELEMETRY_GUIDE.md` for current sensing details


