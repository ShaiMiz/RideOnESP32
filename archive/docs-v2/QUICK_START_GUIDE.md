# ESP32 Power Wheels Controller
## Quick Start Guide

**⚡ Get Up and Running in 30 Minutes**

---

## ⚠️ SAFETY FIRST

- Read full USER_MANUAL.md before starting
- Disconnect battery during all wiring work
- Adult assembly only
- Test all voltages before connecting ESP32

---

## 📦 What You Need

### Essential Hardware:
- ☐ ESP32-WROOM-32U board
- ☐ 4x BTS7960 motor drivers
- ☐ 2x LM2596 buck converters
- ☐ XH-M609 battery protection
- ☐ 30A circuit breaker
- ☐ Jumper AION-RX-MINI receiver
- ☐ Jumper T-Lite V2 transmitter
- ☐ 24V batteries (2x 12V in series)

### Tools:
- Soldering iron & solder
- Wire strippers
- Multimeter
- Screwdrivers
- USB cable (for ESP32)

---

## 🔧 Assembly Steps (30 Minutes)

### Step 1: Power Setup (10 min)

```
Battery (+) → Circuit Breaker → XH-M609 IN+
Battery (-) → Common GND → XH-M609 IN-

XH-M609 OUT+ → LM2596 #1 IN → LM2596 #2 IN → All BTS7960 B+
XH-M609 OUT- → Common GND
```

**Critical:**
1. Set LM2596 #1 to **exactly 3.3V** (measure with multimeter!)
2. Set LM2596 #2 to **exactly 12V**
3. Test voltages BEFORE connecting ESP32

### Step 2: Voltage Divider (5 min)

```
Battery (+) ─[100kΩ]─┬─[12kΩ]─ GND
                     │
                     └─→ ESP32 GPIO 35

⚠️ CRITICAL: Measure voltage at GPIO 35
Must be <3.3V or ESP32 will be DESTROYED!
Expected: ~2.5V at 24V battery
```

### Step 3: ESP32 Connections (10 min)

**Power:**
```
LM2596 #1 (3.3V) → ESP32 VIN
Common GND → ESP32 GND
```

**Motor Enable (all 4 BTS7960):**
```
ESP32 GPIO 21 → All BTS7960 R_EN + L_EN pins
```

**Motor PWM (repeat for all 4):**
```
Motor    RPWM      LPWM
FL:   GPIO 25  GPIO 26
FR:   GPIO 27  GPIO 14
RL:   GPIO 12  GPIO 13
RR:   GPIO 32  GPIO 33
```

**ELRS Receiver:**
```
AION-RX-MINI     ESP32
GND         →    GND
VCC         →    5V (from LM2596 #2 + regulator)
TX (CRSF)   →    GPIO 16 (RX2)
RX          →    GPIO 17 (TX2)
```

### Step 4: BTS7960 Motor Drivers (5 min per driver)

**For EACH of 4 drivers:**

**Control Pins:**
```
BTS7960        ESP32
VCC       →    3.3V (from LM2596 #1)
GND       →    Common GND
RPWM      →    See GPIO table above
LPWM      →    See GPIO table above
R_EN      →    GPIO 21 (shared with all 4)
L_EN      →    GPIO 21 (shared with all 4)
```

**Power Pins:**
```
B+    →    24V from XH-M609
B-    →    Common GND
M+    →    Motor positive wire
M-    →    Motor negative wire
```

---

## 💻 Software Upload (10 min)

### Step 1: Install Software

**Option A: PlatformIO (Recommended)**
1. Install VSCode: https://code.visualstudio.com/
2. Install PlatformIO extension
3. Open project folder
4. Click "Upload"

**Option B: Arduino IDE**
1. Install Arduino IDE 2.x
2. Add ESP32 board support (see USER_MANUAL.md Section 8.2)
3. Install AlfredoCRSF library
4. Open sketch_jun22b.ino
5. Select "ESP32 Dev Module"
6. Click Upload

### Step 2: Upload Firmware

1. Connect ESP32 to computer via USB
2. Click Upload/Build button
3. Wait for "Success" message
4. Open Serial Monitor (115200 baud)

**Expected Output:**
```
[100 ms] ESP32 Power Wheels Controller Starting...
[200 ms] CRSF receiver initialized on Serial2
[300 ms] BTS7960 motor drivers enabled
[500 ms] Setup complete. System ready.
```

---

## ✅ First Power-On Checklist

### Before Connecting Battery:

☐ All wiring connections tight and insulated  
☐ LM2596 #1 outputs exactly 3.3V  
☐ LM2596 #2 outputs exactly 12V  
☐ Voltage divider <3.3V at GPIO 35  
☐ No shorts between power and ground  
☐ Motor wires not touching each other  
☐ Circuit breaker accessible and functional  

### Power-On Sequence:

1. **Close circuit breaker**
2. **Watch Serial Monitor:**
   - Should see initialization messages
   - No error messages
3. **Check ELRS receiver:**
   - LED should be solid (if bound) or flashing (searching)
4. **Bind transmitter:**
   - Put transmitter in bind mode
   - Power on receiver
   - Wait for solid LED

---

## 🎮 First Drive Test

### Test in This Order:

**1. Manual Mode Test:**
- ☐ Shifter to PARK → No movement
- ☐ Shifter to DRIVE → Press pedal → Forward movement
- ☐ Shifter to REVERSE → Press pedal → Reverse movement
- ☐ Release pedal → Vehicle stops smoothly

**2. Remote Mode Test:**
- ☐ Turn on transmitter
- ☐ Switch to REMOTE mode
- ☐ Move throttle stick → Vehicle responds
- ☐ Test speed modes (3-position switch)
- ☐ Test lights control

**3. Failsafe Test:**
- ☐ Drive in remote mode
- ☐ Turn off transmitter
- ☐ Vehicle should stop within 1 second ✅

---

## 🚨 Common First-Time Issues

### Problem: ESP32 won't power on
- **Check:** LM2596 #1 set to 3.3V
- **Check:** Wiring from LM2596 to ESP32 VIN

### Problem: Motors don't run
- **Check:** GPIO 21 (motor enable) connected to all BTS7960
- **Check:** Serial Monitor for error messages
- **Check:** Battery voltage >21V

### Problem: ELRS won't bind
- **Solution:** Put transmitter in bind mode FIRST, then power on receiver
- **Check:** Receiver LED flashing (searching for transmitter)

### Problem: Wrong direction
- **Solution:** Swap M+ and M- wires on that motor

---

## 📊 Quick Reference

### GPIO Pin Summary:

| Function | GPIO Pin(s) |
|----------|-------------|
| Motor Enable | 21 (all 4 drivers) |
| FL Motor | 25 (RPWM), 26 (LPWM) |
| FR Motor | 27 (RPWM), 14 (LPWM) |
| RL Motor | 12 (RPWM), 13 (LPWM) |
| RR Motor | 32 (RPWM), 33 (LPWM) |
| ELRS RX | 16 (RX2), 17 (TX2) |
| Battery Voltage | 35 (ADC) |
| Pedal | 34 (ADC) |
| Shifter D | 39 |
| Shifter R | 5 |

### Voltage Reference:

| Point | Expected Voltage |
|-------|------------------|
| Battery | 24V (19V-27V range) |
| LM2596 #1 | 3.3V (±0.05V) |
| LM2596 #2 | 12V (±0.2V) |
| GPIO 35 | ~2.5V (at 24V battery) |
| BTS7960 B+ | 24V |
| BTS7960 VCC | 3.3V |

---

## 🎯 Next Steps

Once basic operation is confirmed:

1. **Read Full Manual:** USER_MANUAL.md for detailed operation
2. **Calibrate Settings:** Adjust safety thresholds if needed
3. **Add Optional Features:**
   - MCP23017 I/O expander (individual thermal monitoring)
   - SSD1306 OLED display (telemetry dashboard)
4. **Test All Safety Systems:**
   - Low voltage cutoff
   - Overcurrent protection
   - Thermal shutdown
   - Radio failsafe

---

## 📚 Additional Resources

- **Full Manual:** USER_MANUAL.md (60 pages, comprehensive)
- **Wiring Details:** WIRING_CHEAT_SHEET.md (visual diagrams)
- **Safety:** SAFETY_CHECKLIST.md (pre-operation checks)
- **Troubleshooting:** USER_MANUAL.md Section 11
- **Component Specs:** USER_MANUAL.md Section 3

---

## ⚠️ Remember

- **Always supervise children** during operation
- **Test failsafe** before each use
- **Check battery voltage** before driving
- **Inspect wiring** regularly
- **When in doubt, STOP and check the manual!**

---

**Document Version:** 1.0  
**Last Updated:** December 2025  
**Companion to:** ESP32 Power Wheels Controller User Manual v2.1

**Ready to drive? Have fun and stay safe! 🚗💨**







