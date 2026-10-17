# BTS7960 Motor Driver Wiring Guide - 24V System

## 🔌 Complete Wiring for Can-Am Maverick X3 ESP32 Conversion

This guide is specific to your hardware configuration based on the [Component_Descriptions_and_Purpose.pdf](file:///c%3A/Users/Shai%20Mizrahi/Documents/RideOnESP32/Component_Descriptions_and_Purpose.pdf).

---

## 📊 System Overview

```
24V Battery
    ↓
30A Circuit Breaker
    ↓
XH-M609 Battery Protection
    ↓
    ├──→ 4x BTS7960 Motor Drivers → 4x 24V 200W Motors
    ├──→ LM2596 #1 (3.3V) → ESP32-WROOM-32U
    └──→ LM2596 #2 (12V) → Accessories / ELRS Receiver (optional 5V regulator)
```

---

## 🔋 Power Distribution

### Battery → Protection → Distribution

```
[24V Battery]
      |
      | (Main Power Line - Use 12-14 AWG wire)
      |
[30A Resettable Circuit Breaker]
      |
      | (Protected Power)
      |
[XH-M609 Battery Discharge Protection]
  (Cuts off at ~20V to protect battery)
      |
      ├─────────────────────────────────────┐
      |                                     |
      | (24V Protected Output)              |
      |                                     |
  [LM2596 #1]                          [LM2596 #2]
   Output: 3.3V                         Output: 12V
      |                                     |
      ├──→ ESP32 VIN (3.3V)                ├──→ Accessories (12V)
      └──→ ESP32 GND                       └──→ Optional: 5V regulator for ELRS
```

**⚠️ CRITICAL**: All grounds must be common!

---

## 🚗 BTS7960 Motor Driver Pinout

Each BTS7960 module has the following pins:

### Power Pins
| Pin | Description | Connect To |
|-----|-------------|------------|
| **B+** | Motor power positive | 24V from XH-M609 output |
| **B-** | Motor power ground | Battery negative (common ground) |

### Motor Output Pins
| Pin | Description | Connect To |
|-----|-------------|------------|
| **M+** | Motor positive terminal | Motor + wire |
| **M-** | Motor negative terminal | Motor - wire |

### Control Pins (5V logic)
| Pin | Description | Connect To |
|-----|-------------|------------|
| **VCC** | Logic power (5V) | 5V supply or 3.3V (see note below) |
| **GND** | Logic ground | Common ground |
| **RPWM** | Right PWM (forward) | ESP32 GPIO (see table below) |
| **LPWM** | Left PWM (reverse) | ESP32 GPIO (see table below) |
| **R_EN** | Right enable | VCC (always enabled) or GPIO 21 |
| **L_EN** | Left enable | VCC (always enabled) or GPIO 21 |
| **R_IS** | Right current sense | ESP32 ADC (optional) |
| **L_IS** | Left current sense | ESP32 ADC (optional) |

**⚠️ VCC Voltage Note:**
- BTS7960 logic is **5V tolerant** but works with 3.3V
- **Option 1**: Power from 5V regulator (better for noisy environments)
- **Option 2**: Power from ESP32 3.3V (simpler, usually works fine)
- PWM inputs are 3.3V compatible when VCC is 3.3V

---

## 📌 ESP32 to BTS7960 Connections

### Front-Left Motor (BTS7960 #1)
| BTS7960 Pin | ESP32 Pin | Description |
|-------------|-----------|-------------|
| VCC | 3.3V or 5V | Logic power |
| GND | GND | Ground |
| RPWM | **GPIO 25** | Forward PWM |
| LPWM | **GPIO 26** | Reverse PWM |
| R_EN | VCC or GPIO 21 | Enable |
| L_EN | VCC or GPIO 21 | Enable |
| B+ | 24V (XH-M609) | Motor power |
| B- | GND | Ground |
| M+ | Motor + | Front-left motor positive |
| M- | Motor - | Front-left motor negative |

### Front-Right Motor (BTS7960 #2)
| BTS7960 Pin | ESP32 Pin | Description |
|-------------|-----------|-------------|
| RPWM | **GPIO 27** | Forward PWM |
| LPWM | **GPIO 14** | Reverse PWM |
| (Other pins same as Front-Left) | | |

### Rear-Left Motor (BTS7960 #3)
| BTS7960 Pin | ESP32 Pin | Description |
|-------------|-----------|-------------|
| RPWM | **GPIO 12** | Forward PWM (⚠️ boot pin) |
| LPWM | **GPIO 13** | Reverse PWM |
| (Other pins same as Front-Left) | | |

**⚠️ GPIO 12 Warning**: This pin is used during ESP32 boot. If you have boot issues, change to a different GPIO.

### Rear-Right Motor (BTS7960 #4)
| BTS7960 Pin | ESP32 Pin | Description |
|-------------|-----------|-------------|
| RPWM | **GPIO 32** | Forward PWM |
| LPWM | **GPIO 33** | Reverse PWM |
| (Other pins same as Front-Left) | | |

---

## 🎮 ELRS Receiver Connection

Based on [ESP32_ELRS_Receiver_Connection_Guide.pdf](file:///c%3A/Users/Shai%20Mizrahi/Documents/RideOnESP32/ESP32_ELRS_Receiver_Connection_Guide.pdf):

### ELRS Nano 2.4GHz Receiver

```
ELRS Receiver        ESP32
┌──────────┐        ┌──────────┐
│          │        │          │
│  VCC  ───┼────────┤ 5V or    │ (See power options below)
│          │        │ 3.3V     │
│  GND  ───┼────────┤ GND      │
│          │        │          │
│  TX   ───┼────────┤ GPIO 16  │ (CRSF data to ESP32)
│  (CRSF)  │        │ (RX2)    │
│          │        │          │
│  RX   ───┼────────┤ GPIO 17  │ (Optional - telemetry back)
│          │        │ (TX2)    │
└──────────┘        └──────────┘
```

| Receiver Pin | ESP32 Pin | Description |
|--------------|-----------|-------------|
| VCC | 5V or 3.3V | Power (see below) |
| GND | GND | Ground |
| TX (CRSF) | GPIO 16 (Serial2 RX) | CRSF data output |
| RX | GPIO 17 (Serial2 TX) | Telemetry input (optional) |

### ELRS Receiver Power Options

**Option 1: 5V Power (Recommended)**
- Better signal strength and range
- More reliable in electrically noisy environments (motors create EMI)
- Add small 5V regulator from 12V LM2596 output
- Example: LM7805 or AMS1117-5.0

```
12V LM2596 → [5V Regulator] → ELRS VCC
                    ↓
                   GND
```

**Option 2: 3.3V Power (Simpler)**
- Share ESP32 3.3V supply
- Works for most ELRS receivers
- Simpler wiring
- May have slightly reduced range

```
ESP32 3.3V → ELRS VCC
ESP32 GND  → ELRS GND
```

**My Recommendation**: Use 5V for the receiver. The 24V motors will create significant electrical noise, and 5V will give you better immunity.

---

## 🔌 Complete Pin Assignment Table

### ESP32 GPIO Pin Usage

| GPIO | Function | Connected To | Direction |
|------|----------|--------------|-----------|
| 16 | Serial2 RX | ELRS TX (CRSF) | Input |
| 17 | Serial2 TX | ELRS RX (optional) | Output |
| 21 | Motor Enable | BTS7960 R_EN + L_EN (all 4) | Output |
| 25 | FL_RPWM | Front-Left forward PWM | Output |
| 26 | FL_LPWM | Front-Left reverse PWM | Output |
| 27 | FR_RPWM | Front-Right forward PWM | Output |
| 14 | FR_LPWM | Front-Right reverse PWM | Output |
| 12 | RL_RPWM | Rear-Left forward PWM | Output |
| 13 | RL_LPWM | Rear-Left reverse PWM | Output |
| 32 | RR_RPWM | Rear-Right forward PWM | Output |
| 33 | RR_LPWM | Rear-Right reverse PWM | Output |
| 35 | ADC | Battery voltage (via divider) | Input |
| 36 | ADC | Current sensor (optional) | Input |
| 19 | Digital Input | Over-temp fault (optional) | Input |
| 34 | ADC | Pedal potentiometer | Input |
| 39 | Digital Input | Shifter Drive position | Input |
| 5 | Digital Input | Shifter Reverse position | Input |
| 22 | Digital Output | Lights control | Output |
| 23 | Digital Output | Audio play (future) | Output |
| 18 | Digital Output | Audio vol+ (future) | Output |
| 4 | Digital Output | Audio vol- (future) | Output |

---

## 🔧 Voltage Divider for 24V Battery Monitoring

**⚠️ CRITICAL**: ESP32 ADC max voltage is **3.3V**. Exceeding this will **destroy the ESP32**!

### For 24V System: Use 100kΩ + 12kΩ Divider

```
24V Battery +
    │
   [100kΩ] ←─ R1
    │
    ├──────────→ ESP32 GPIO 35 (ADC)
    │
   [12kΩ]  ←─ R2
    │
   GND
```

**Divider Ratio**: (100k + 12k) / 12k = **9.333**

**Voltage Calculations:**
- At 24V: 24 / 9.333 = **2.57V** at GPIO 35 ✅ (safe)
- At 27V (full charge): 27 / 9.333 = **2.89V** ✅ (safe)
- At 30V (max): 30 / 9.333 = **3.21V** ✅ (safe, below 3.3V)

**Component Specs:**
- R1: 100kΩ resistor (1/4W or higher)
- R2: 12kΩ resistor (1/4W or higher)
- Use 1% tolerance resistors for accuracy

**⚠️ DO NOT use 82kΩ + 12kΩ** (from 12V system) - this would give 3.06V at 24V, which is too close to the 3.3V limit!

---

## ⚡ Current Sensing Options

### Option 1: External ACS712-30B Sensor (Recommended)

For 24V system with 4x 200W motors:
- Total power: 800W
- Total current: 800W / 24V = **33.3A typical**
- Peak current: Can exceed 50A during acceleration

**Use ACS712-30B** (±30A range, 66mV/A sensitivity)

```
Battery + ──→ [ACS712 IP+] ───→ XH-M609 / Motor Drivers
                  │
Battery - ────────┤ IP-
                  │
        5V ───────┤ VCC
       GND ───────┤ GND
                  │
    [Voltage      │
     Divider] ────┤ OUT (0-5V)
        │         │
ESP32 GPIO 36 ────┘
```

**⚠️ ACS712 outputs 0-5V**, but ESP32 ADC max is 3.3V!

**Add voltage divider on ACS712 output:**
```
ACS712 OUT (0-5V)
    │
   [10kΩ]
    │
    ├──────→ ESP32 GPIO 36
    │
   [22kΩ]
    │
   GND
```
This scales 5V → 3.4V (safe for ESP32)

### Option 2: BTS7960 Built-in Current Sense

BTS7960 has R_IS and L_IS pins that output voltage proportional to current:
- Sensitivity: ~1mV per 1A (check your module's datasheet)
- Output range: 0-3.3V (safe for ESP32)
- Connect R_IS or L_IS to ESP32 ADC pins

**Advantage**: No external sensor needed  
**Disadvantage**: Need to read 4 separate sensors (one per motor)

---

## 🔌 Complete Wiring Checklist

### Power System
- [ ] 24V battery connected to 30A circuit breaker
- [ ] Circuit breaker connected to XH-M609 input
- [ ] XH-M609 output connected to:
  - [ ] All 4 BTS7960 B+ pins (24V)
  - [ ] LM2596 #1 input (for 3.3V)
  - [ ] LM2596 #2 input (for 12V)
- [ ] LM2596 #1 output set to **3.3V** (measure with multimeter!)
- [ ] LM2596 #2 output set to **12V**
- [ ] All grounds connected together (common ground)

### ESP32 Power
- [ ] ESP32 VIN connected to LM2596 #1 (3.3V)
- [ ] ESP32 GND connected to common ground

### BTS7960 Motor Drivers (x4)
For each driver:
- [ ] B+ connected to 24V (XH-M609 output)
- [ ] B- connected to common ground
- [ ] VCC connected to 3.3V or 5V
- [ ] GND connected to common ground
- [ ] RPWM connected to ESP32 GPIO (see table)
- [ ] LPWM connected to ESP32 GPIO (see table)
- [ ] R_EN and L_EN connected to VCC or GPIO 21
- [ ] M+ and M- connected to motor

### ELRS Receiver
- [ ] VCC connected to 5V (recommended) or 3.3V
- [ ] GND connected to common ground
- [ ] TX connected to ESP32 GPIO 16
- [ ] RX connected to ESP32 GPIO 17 (optional)
- [ ] Receiver bound to Jumper T-Lite V2 transmitter

### Voltage Divider (Battery Monitor)
- [ ] 100kΩ resistor from battery + to junction
- [ ] 12kΩ resistor from junction to GND
- [ ] Junction connected to ESP32 GPIO 35
- [ ] **Verified with multimeter**: <3.3V at GPIO 35 when battery at max voltage

### Current Sensor (Optional)
- [ ] ACS712-30B installed in battery + line
- [ ] Voltage divider on ACS712 output (if using 5V sensor)
- [ ] Output connected to ESP32 GPIO 36

### Manual Controls
- [ ] Pedal potentiometer: one end to 3.3V, other to GND, wiper to GPIO 34
- [ ] Shifter D switch: one side to GPIO 39, other to GND
- [ ] Shifter R switch: one side to GPIO 5, other to GND

### Lights
- [ ] Lights relay or MOSFET controlled by GPIO 22
- [ ] Lights powered from 12V LM2596 output

---

## 🧪 Testing Procedure

### 1. Power System Test (NO ESP32 connected yet)
1. Set LM2596 #1 to exactly **3.3V** using multimeter
2. Set LM2596 #2 to exactly **12V** using multimeter
3. Measure voltage divider output at GPIO 35 connection point
   - Should be ~2.57V with 24V battery
   - **MUST be <3.3V** - if not, STOP and fix divider!

### 2. ESP32 Power Test
1. Connect ESP32 to 3.3V supply
2. Upload test firmware with `DEVELOPMENT_MODE = true`
3. Verify Serial output shows startup messages
4. Verify PWM channels initialize successfully

### 3. BTS7960 Test (Motors Disconnected)
1. Connect BTS7960 modules
2. Upload firmware
3. Test each motor driver individually:
   - Set RPWM to 50% - measure voltage at M+ (should be ~12V)
   - Set LPWM to 50% - measure voltage at M- (should be ~12V)
   - Set both to 0 - M+ and M- should be 0V

### 4. Motor Test (No Load)
1. Connect motors (wheels off ground)
2. Test manual mode:
   - Shifter in PARK → motors should NOT spin
   - Shifter in DRIVE + pedal → motors spin forward slowly
   - Shifter in REVERSE + pedal → motors spin reverse
3. Verify smooth acceleration (ramping working)

### 5. ELRS Test
1. Power on transmitter (Jumper T-Lite V2)
2. Verify receiver binds/connects (LED indicator)
3. Switch to remote mode
4. Test throttle control
5. Test speed mode switch (3 positions)
6. **Test failsafe**: Turn off transmitter, verify motors stop within 1 second

### 6. Full System Test
1. Place vehicle on ground
2. Test both control modes
3. Monitor Serial output for telemetry
4. Verify all safety systems active
5. Check for overheating after 5-10 minutes of operation

---

## ⚠️ Safety Warnings

1. **ALWAYS disconnect battery when wiring**
2. **NEVER exceed 3.3V on ESP32 GPIO pins**
3. **Use 30A fuse/breaker on battery positive line** (already in your system)
4. **Verify voltage divider output <3.3V BEFORE connecting to ESP32**
5. **Test BTS7960 drivers without motors first**
6. **Ensure all grounds are common** (ESP32, battery, drivers, receiver)
7. **Mount ESP32 away from motors** (EMI protection)
8. **Use heat sinks on BTS7960 modules** (they can get hot at high current)
9. **Secure all wiring away from wheels and moving parts**
10. **Test failsafe before giving vehicle to child**

---

## 📚 Reference Documents

- [Component_Descriptions_and_Purpose.pdf](file:///c%3A/Users/Shai%20Mizrahi/Documents/RideOnESP32/Component_Descriptions_and_Purpose.pdf) - Your hardware specifications
- [ESP32_ELRS_Receiver_Connection_Guide.pdf](file:///c%3A/Users/Shai%20Mizrahi/Documents/RideOnESP32/ESP32_ELRS_Receiver_Connection_Guide.pdf) - ELRS receiver wiring
- README.md - Software configuration and features
- WIRING_GUIDE.md - General wiring information

---

## 🔧 Troubleshooting

### Motors Don't Spin
- Check BTS7960 B+ has 24V
- Verify RPWM/LPWM signals with multimeter (should show ~1.65V at 50% PWM)
- Check R_EN and L_EN are HIGH (connected to VCC or GPIO 21 HIGH)
- Verify motor connections (M+ and M-)

### Motors Spin Wrong Direction
- Swap M+ and M- wires on motor
- OR swap RPWM and LPWM pin definitions in code

### ESP32 Won't Boot
- GPIO 12 (RL_RPWM) may be interfering with boot
- Disconnect GPIO 12 temporarily and test
- If needed, change RL_RPWM to different GPIO (e.g., GPIO 15)

### ELRS Receiver Not Connecting
- Verify receiver is bound to transmitter
- Check power to receiver (should be 3.3V or 5V)
- Verify TX pin connected to ESP32 GPIO 16 (RX2)
- Check Serial2 baud rate is 420000 in code

### Battery Voltage Reading Incorrect
- Measure actual battery voltage with multimeter
- Measure voltage at GPIO 35 (should be battery / 9.333)
- Verify resistor values (100kΩ and 12kΩ)
- Adjust `VOLTAGE_DIVIDER_RATIO` in code if needed

---

**Questions?** Review the troubleshooting sections in README.md or check Serial Monitor output for diagnostic messages.

