# Wiring Guide - ESP32 Power Wheels Controller

## 🔌 Complete Wiring Reference

This guide provides detailed wiring instructions for connecting the ESP32 to all components.

---

## ⚡ Power Supply

### ESP32 Power
- **5V Power**: Connect to ESP32 VIN pin (or 5V pin on some boards)
- **GND**: Connect to ESP32 GND
- **Source**: Use a 5V BEC (Battery Eliminator Circuit) from main battery
  - Recommended: 5V/3A BEC for stable power
  - Alternative: Separate 5V power supply

### Motor Driver Power
- **Main Battery**: Connect directly to motor driver VIN (12V-24V depending on setup)
- **GND**: Common ground with ESP32
- **Fuse**: **REQUIRED** - Add appropriate fuse (20A-30A) on positive wire

---

## 🚗 Motor Drivers (4x H-Bridge)

### Typical H-Bridge Connections (per motor)
```
Motor Driver Input Side:
┌─────────────────────┐
│  H-Bridge Module    │
│                     │
│  VCC ←── 12V Battery│
│  GND ←── Ground     │
│  PWM ←── ESP32 GPIO │
│  DIR ←── ESP32 GPIO │
│  EN  ←── MOTOR_EN   │
│                     │
│  OUT+ ──→ Motor +   │
│  OUT- ──→ Motor -   │
└─────────────────────┘
```

### Front-Left Motor Driver
| Motor Driver Pin | Connect To | Description |
|------------------|------------|-------------|
| VCC | Battery + (via fuse) | Motor power supply |
| GND | Battery - (common ground) | Ground |
| PWM | ESP32 GPIO 25 (FL_PWM) | Speed control |
| DIR | ESP32 GPIO 26 (FL_DIR) | Direction control |
| EN | ESP32 GPIO 21 (MOTOR_ENABLE) | Enable signal |
| OUT+ | Motor + terminal | Motor positive |
| OUT- | Motor - terminal | Motor negative |

### Front-Right Motor Driver
| Motor Driver Pin | Connect To |
|------------------|------------|
| PWM | ESP32 GPIO 27 (FR_PWM) |
| DIR | ESP32 GPIO 14 (FR_DIR) |
| (Other pins same as Front-Left) |

### Rear-Left Motor Driver
| Motor Driver Pin | Connect To |
|------------------|------------|
| PWM | ESP32 GPIO 12 (RL_PWM) |
| DIR | ESP32 GPIO 13 (RL_DIR) |
| (Other pins same as Front-Left) |

### Rear-Right Motor Driver
| Motor Driver Pin | Connect To |
|------------------|------------|
| PWM | ESP32 GPIO 32 (RR_PWM) |
| DIR | ESP32 GPIO 33 (RR_DIR) |
| (Other pins same as Front-Left) |

**⚠️ Important Notes:**
- All 4 motor drivers share MOTOR_ENABLE (GPIO 21)
- All grounds must be common (ESP32, battery, motor drivers)
- GPIO 12 has bootloader usage - if problems arise, change to different pin

---

## 📡 CRSF Receiver (ELRS)

### AION-RX-MINI or Similar ELRS Receiver
```
ELRS Receiver        ESP32
┌──────────┐        ┌──────────┐
│          │        │          │
│  5V   ───┼────────┤ 5V       │
│  GND  ───┼────────┤ GND      │
│  TX   ───┼────────┤ GPIO 16  │ (ESP32 RX)
│  RX   ───┼────────┤ GPIO 17  │ (ESP32 TX)
│          │        │          │
└──────────┘        └──────────┘
```

| Receiver Pin | ESP32 Pin | Description |
|--------------|-----------|-------------|
| 5V | 5V | Power supply (from BEC or ESP32) |
| GND | GND | Ground |
| TX | GPIO 16 (Serial2 RX) | CRSF data to ESP32 |
| RX | GPIO 17 (Serial2 TX) | ESP32 to receiver (optional) |

**Configuration:**
- UART2 on ESP32 (RX=16, TX=17)
- Baud rate: 420000 (CRSF standard)
- Protocol: CRSF (configured in ELRS transmitter)

---

## 🔋 Battery Voltage Monitor

### Voltage Divider Circuit
```
Battery + (12V)
    │
   [82kΩ]  ←─ R1
    │
    ├──────────→ ESP32 GPIO 35 (ADC)
    │
   [12kΩ]  ←─ R2
    │
   GND
```

**Component List:**
- R1: 82kΩ resistor (1/4W or higher)
- R2: 12kΩ resistor (1/4W or higher)
- Connection: Junction between resistors → GPIO 35

**Divider Ratio**: (82k + 12k) / 12k = 7.833
- 12V battery → 1.53V at GPIO 35 ✅ (safe, <3.3V max)
- 16.8V (4S LiPo full) → 2.14V at GPIO 35 ✅

**⚠️ WARNING**: 
- ESP32 ADC max voltage is **3.3V** - exceeding this will **damage the ESP32**!
- For higher voltages (5S+), use larger R1 value

**Calculation for Different Voltages:**
```
R1 = R2 × ((V_max / 3.0) - 1)
Example: 24V max with R2=12kΩ
R1 = 12k × ((24 / 3.0) - 1) = 12k × 7 = 84kΩ (use 82kΩ + 2.2kΩ in series)
```

---

## ⚡ Current Sensor (ACS712)

### ACS712 Hall-Effect Current Sensor
```
Battery + ──→ [ACS712 IP+] ───→ Motor Drivers
                  │
Battery - ────────┤ IP-
                  │
        5V ───────┤ VCC
       GND ───────┤ GND
ESP32 GPIO 36 ────┤ OUT (analog output)
```

| ACS712 Pin | Connect To | Description |
|------------|------------|-------------|
| VCC | 5V | Sensor power supply |
| GND | GND | Ground |
| OUT | ESP32 GPIO 36 | Analog output (0-5V) |
| IP+ | Battery + (positive line) | Current flows through sensor |
| IP- | Motor drivers VIN | Current return path |

**⚠️ ACS712 Output Voltage:**
- Output range: 0V to 5V
- ESP32 ADC max: **3.3V**
- **Problem**: 5V output will damage ESP32!

**Solution**: Add voltage divider on ACS712 output:
```
ACS712 OUT
    │
   [10kΩ]
    │
    ├────────→ ESP32 GPIO 36
    │
   [22kΩ]
    │
   GND
```
This reduces 5V to 3.4V (adjust for exactly 3.3V max)

**Alternative**: Use 3.3V-compatible current sensor (INA219, INA226)

**Sensor Selection:**
- ACS712-05B: ±5A range (185mV/A) - for small motors
- ACS712-20B: ±20A range (100mV/A) - **recommended for Power Wheels**
- ACS712-30B: ±30A range (66mV/A) - for high-power motors

---

## 🌡️ Temperature Monitoring

### Motor Driver Over-Temp Fault Pin
```
Motor Driver          ESP32
┌──────────┐        ┌──────────┐
│          │        │          │
│  FAULT ──┼────────┤ GPIO 19  │
│          │        │          │
│  (Active │        │ INPUT_   │
│   LOW)   │        │ PULLUP   │
│          │        │          │
└──────────┘        └──────────┘
```

| Motor Driver Pin | ESP32 Pin | Description |
|------------------|-----------|-------------|
| FAULT (or DIAG) | GPIO 19 | Over-temp fault signal |

**Behavior:**
- Normal operation: FAULT pin is HIGH (open or 5V)
- Over-temperature: FAULT pin pulls LOW (0V)
- ESP32 uses INPUT_PULLUP mode
  - Reads HIGH normally
  - Reads LOW when fault active

**If your motor driver doesn't have fault pin:**
- Leave GPIO 19 unconnected (firmware will read HIGH always)
- Consider adding external thermistor on motor/driver heatsink

---

## 🎮 Manual Control Inputs

### Pedal Potentiometer
```
         3.3V
          │
     ┌────┴────┐
     │  POT    │
     │  ┌─┐    │
     │  │◄│←── Wiper
     │  └─┘    │
     └────┬────┘
          │
         GND
          │
          ├──────→ ESP32 GPIO 34 (ADC)
```

| Potentiometer Pin | Connect To | Description |
|-------------------|------------|-------------|
| Pin 1 | 3.3V | Supply voltage |
| Pin 2 (wiper) | ESP32 GPIO 34 | Pedal position signal |
| Pin 3 | GND | Ground |

**Potentiometer Specs:**
- Value: 5kΩ to 10kΩ (linear taper recommended)
- Power: 1/4W or higher
- Output: 0V (not pressed) to 3.3V (fully pressed)

**⚠️ Use 3.3V, NOT 5V**: ESP32 ADC max is 3.3V

---

### Physical Shifter (Drive/Reverse/Park)
```
Shifter Switch (Drive)
┌─────────┐
│  D  ────┼───→ ESP32 GPIO 39 (INPUT_PULLUP)
│  COM ───┼───→ GND
└─────────┘

Shifter Switch (Reverse)
┌─────────┐
│  R  ────┼───→ ESP32 GPIO 5 (INPUT_PULLUP)
│  COM ───┼───→ GND
└─────────┘
```

| Switch | ESP32 Pin | State | GPIO Reading |
|--------|-----------|-------|--------------|
| Drive | GPIO 39 | Not engaged | HIGH (pulled up) |
| Drive | GPIO 39 | **Engaged** | **LOW** (shorted to GND) |
| Reverse | GPIO 5 | Not engaged | HIGH (pulled up) |
| Reverse | GPIO 5 | **Engaged** | **LOW** (shorted to GND) |

**Shifter Logic:**
- D=LOW, R=HIGH → **DRIVE** mode
- D=HIGH, R=LOW → **REVERSE** mode
- D=HIGH, R=HIGH → **PARK** mode (default)
- D=LOW, R=LOW → **PARK** mode (invalid, safe default)

**Switch Type:**
- SPST (Single Pole Single Throw)
- NO (Normally Open) or NC (Normally Closed) depending on shifter design
- Adjust wiring to match active-low logic (LOW when engaged)

---

## 💡 Lights Control

### LED Strips or Headlights (with MOSFET/Relay)
```
ESP32 GPIO 22
    │
    ├───[1kΩ]───→ MOSFET Gate (e.g., IRF540N)
    │
   GND

MOSFET:
    Drain  ←─── LED Strip + (or relay coil)
    Source ──→ GND
    
Battery + ───→ LED Strip -
```

**Simple Relay Version:**
| Relay Module Pin | Connect To |
|------------------|------------|
| VCC | 5V |
| GND | GND |
| IN | ESP32 GPIO 22 |
| COM | Battery + (via fuse) |
| NO | Lights + |

**Lights - connected to:** GND

**Logic:**
- GPIO 22 HIGH → Lights ON
- GPIO 22 LOW → Lights OFF

---

## 🔊 Audio System (Optional - Not Yet Implemented)

### DFPlayer Mini MP3 Module (Placeholder)
```
DFPlayer Mini        ESP32
┌──────────┐        ┌──────────┐
│  VCC  ───┼────────┤ 5V       │
│  GND  ───┼────────┤ GND      │
│  BUSY ───┼────────┤ GPIO 23  │ (Optional: Play status)
│          │        │          │
│ Speaker+ ──→ Speaker
│ Speaker- ──→ Speaker
│          │
│ (SD Card with MP3 files)
└──────────┘
```

**⚠️ Audio pins defined but not implemented in current firmware version**

Pins defined for future use:
- AUDIO_PLAY_PIN: GPIO 23
- AUDIO_VOLUP_PIN: GPIO 18
- AUDIO_VOLDOWN_PIN: GPIO 4

---

## 🔗 Common Ground

**⚠️ CRITICAL: All grounds must be connected together:**
- ESP32 GND
- Battery -
- Motor driver GND
- CRSF receiver GND
- Voltage divider GND
- Current sensor GND
- All other component GNDs

**Single Ground Point Recommended**: Connect all grounds to a single terminal block or battery negative terminal to avoid ground loops.

---

## 📋 Complete Shopping List

### Required Components
- [ ] ESP32-WROOM-32 development board
- [ ] ELRS receiver (e.g., AION-RX-MINI)
- [ ] 4x H-bridge motor drivers (e.g., BTS7960 or L298N)
- [ ] 5V/3A BEC (Battery Eliminator Circuit)
- [ ] Voltage divider resistors: 82kΩ, 12kΩ (1/4W)
- [ ] Battery: 12V lead-acid or 3S/4S LiPo
- [ ] Fuse: 20-30A automotive blade fuse + holder
- [ ] Hookup wire: 18-22 AWG for signals, 12-14 AWG for power
- [ ] Connectors: JST, XT60, or similar for battery

### Optional Components
- [ ] Current sensor: ACS712-20B (or INA219 for 3.3V compatibility)
- [ ] Voltage divider for ACS712 output (10kΩ, 22kΩ)
- [ ] LED lights/strips
- [ ] MOSFET for lights (IRF540N or similar)
- [ ] DFPlayer Mini MP3 module (future audio)
- [ ] Terminal blocks for clean wiring
- [ ] Heat shrink tubing
- [ ] Cable ties

---

## 🧰 Wiring Best Practices

1. **Color Code Your Wires**
   - Red: Positive power
   - Black: Ground
   - Yellow/White: Signal wires
   - Green: Motor wires

2. **Use Appropriate Wire Gauge**
   - Power (battery to motor drivers): 12-14 AWG
   - Motor wires: 14-16 AWG
   - Signal wires (ESP32 GPIOs): 20-22 AWG

3. **Secure All Connections**
   - Solder and heat shrink signal wires
   - Use crimp connectors for power wires
   - Use terminal blocks for easy disconnect

4. **Add Protection**
   - Fuse on battery positive line (required!)
   - Diodes across motor terminals (optional, reduces EMI)
   - Capacitors on motor drivers (check driver specs)

5. **Cable Management**
   - Keep signal wires away from power wires
   - Use cable ties to organize wiring
   - Label all connections for future maintenance

6. **Testing Procedure**
   - Test voltage divider output with multimeter BEFORE connecting to ESP32
   - Test current sensor output with multimeter BEFORE connecting to ESP32
   - Verify all voltages are within ESP32 specs (<3.3V on GPIO pins)
   - Connect motors LAST after software testing

---

## 🔍 Troubleshooting Wiring Issues

### ESP32 Won't Boot
- Check 5V power supply (should be 4.5-5.5V)
- Verify GPIO 12 (RL_PWM) isn't pulled HIGH at boot
- Disconnect motor drivers and test ESP32 alone

### Motors Don't Spin
- Check MOTOR_ENABLE pin (should be HIGH when enabled)
- Verify motor driver power supply (12V present?)
- Check PWM and DIR connections with multimeter/oscilloscope
- Verify common ground between ESP32 and motor drivers

### Incorrect Voltage Reading
- Measure voltage at GPIO 35 with multimeter (should be <3.3V)
- Verify resistor values (82kΩ and 12kΩ)
- Check battery voltage separately to confirm

### Incorrect Current Reading
- Measure ACS712 output voltage (should be ~2.5V at 0A)
- If using voltage divider, verify output is <3.3V
- Check that current is flowing through sensor (IP+ to IP-)

### CRSF Receiver Not Communicating
- Verify receiver is bound to transmitter
- Check TX/RX connections (TX→RX, RX→TX)
- Confirm baud rate is 420000 in code
- Test receiver with LED indicator (if available)

---

## ⚠️ Safety Warnings

1. **ALWAYS disconnect battery when wiring**
2. **NEVER exceed 3.3V on ESP32 GPIO pins**
3. **Use a fuse on battery positive line**
4. **Double-check polarity before applying power**
5. **Test voltage dividers with multimeter before connecting to ESP32**
6. **Keep wiring away from moving parts (gears, wheels)**
7. **Use insulated connectors and heat shrink**
8. **Mount ESP32 in protective enclosure**

---

**Questions or Issues?** Review the Troubleshooting section in README.md or check Serial Monitor output for diagnostic messages.

