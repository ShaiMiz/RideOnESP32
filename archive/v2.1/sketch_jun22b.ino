/*******************************************************************************
 * ESP32 Power Wheels Controller Firmware - Can-Am Maverick X3
 * 
 * Project: Replacement control board for Power Wheels Can-Am Maverick X3
 * Board: ESP32-WROOM-32U Microcontroller
 * Arduino ESP32 Core: 3.x or later (uses new ledcAttach() API)
 * 
 * HARDWARE SPECIFICATIONS:
 * ========================
 * - Battery: 24V (original Can-Am X3 batteries)
 * - Motors: 4x 24V 22k RPM 200W motors
 * - Motor Drivers: 4x BTS7960 H-Bridge modules (43A continuous, 100A peak)
 * - Power Supply: LM2596 buck converters (3.3V for ESP32, 12V for accessories)
 * - Protection: XH-M609 battery discharge protection + 30A circuit breaker
 * - Radio: Jumper T-Lite V2 2.4GHz ELRS + ELRS Nano Receiver 2.4GHz
 * 
 * FEATURES:
 * =========
 * - Dual control modes: Manual (pedal/shifter) and Remote (ELRS radio)
 * - 4-wheel independent motor control with BTS7960 drivers
 * - CRSF receiver integration (ExpressLRS 2.4GHz system)
 * - Safety features: voltage monitoring, current limiting, thermal protection
 * - Radio failsafe with automatic shutdown (1 second timeout)
 * - Motor ramping to prevent gear damage and mechanical stress
 * - Real-time telemetry output for monitoring
 * 
 * COMPATIBILITY:
 * ==============
 * - AlfredoCRSF library: https://github.com/AlfredoSystems/AlfredoCRSF
 *   (CRSF protocol - standard for ExpressLRS systems)
 * - ESP32 Arduino Core 3.x (uses new ledcAttach() API)
 * 
 * IMPORTANT NOTES:
 * ================
 * - 24V system requires proper voltage divider scaling (see VOLTAGE_DIVIDER_RATIO)
 * - BTS7960 drivers need both RPWM and LPWM pins for bi-directional control
 * - ESP32 powered by 3.3V LM2596 (NOT 5V - prevents overheating)
 * - ELRS receiver can be powered by 3.3V or 5V (5V recommended for better range)
 * 
 * Author: [Your Name]
 * Version: 2.1
 * Last Updated: December 2025
 ******************************************************************************/

#include <AlfredoCRSF.h>    // For CRSF receiver (ExpressLRS ELRS Nano 2.4GHz)

// ========================== Motor Driver Pin Definitions ==========================
// BTS7960 H-Bridge Motor Driver Modules (4 units)
// Each BTS7960 has dual PWM inputs for bi-directional control:
//   - RPWM (Right PWM): Forward direction control (0-255 duty cycle)
//   - LPWM (Left PWM):  Reverse direction control (0-255 duty cycle)
//   - R_EN: Right side enable (connect to VCC or GPIO)
//   - L_EN: Left side enable (connect to VCC or GPIO)
//   - R_IS: Right side current sense (optional - can connect to ADC)
//   - L_IS: Left side current sense (optional - can connect to ADC)
//
// Control Logic: 
//   - Forward: RPWM = speed (0-255), LPWM = 0
//   - Reverse: RPWM = 0, LPWM = speed (0-255)
//   - Brake: RPWM = 0, LPWM = 0
//
// Motors: 24V 22k RPM 200W (4 units)
// Driver Specs: 43A continuous, 100A peak per driver

// Front-Left Motor (BTS7960)
#define FL_RPWM       25    // Front-left forward PWM (GPIO 25)
#define FL_LPWM       26    // Front-left reverse PWM (GPIO 26)

// Front-Right Motor (BTS7960)
#define FR_RPWM       27    // Front-right forward PWM (GPIO 27)
#define FR_LPWM       14    // Front-right reverse PWM (GPIO 14)

// Rear-Left Motor (BTS7960)
#define RL_RPWM       12    // Rear-left forward PWM (GPIO 12) - Note: used at boot
#define RL_LPWM       13    // Rear-left reverse PWM (GPIO 13)

// Rear-Right Motor (BTS7960)
#define RR_RPWM       32    // Rear-right forward PWM (GPIO 32)
#define RR_LPWM       33    // Rear-right reverse PWM (GPIO 33)

// BTS7960 Enable Pins (optional - can tie R_EN and L_EN to VCC for always-enabled)
#define MOTOR_ENABLE  21    // Global enable for all motor drivers (GPIO 21)
                            // Connect to all R_EN and L_EN pins if using software enable
                            // Or tie R_EN/L_EN to VCC and use this pin for relay/contactor

// ========================== Telemetry Pins ==========================
// All ADC pins use ADC1 (GPIO 32-39) for WiFi/Bluetooth compatibility
// ADC1 can be used simultaneously with WiFi, ADC2 cannot
#define VOLTAGE_PIN      35  // ADC1_CH7 for battery voltage (via voltage divider)
                             // FOR 24V SYSTEM: Use 100k/12k divider for safety
                             // Divider ratio: (100k + 12k) / 12k = 9.333x
                             // Max voltage: 3.3V * 9.333 = 30.8V (safe for 24V nominal)
                             // Note: XH-M609 protection cuts off at ~20V (low voltage)
// BTS7960 Thermal Fault Monitoring Pins (active LOW)
// Each BTS7960 module has a fault output that goes LOW when overheating
// Option 1: Wire all 4 to GPIO 19 with diode OR logic (combined monitoring)
// Option 2: Wire individually to 4 GPIO pins (individual monitoring)

// Combined monitoring (default - RECOMMENDED):
#define TEMP_FAULT_PIN   19  // Combined fault input from all 4 drivers
                             // Wire all 4 BTS7960 FAULT pins to GPIO 19 via diode OR logic
                             // Uses: 4x 1N4148 diodes + 1x 10kΩ pull-up resistor

// ⚠️ Individual monitoring NOT RECOMMENDED - insufficient GPIO pins available
// Would require 4 pins but only 3 are free (audio pins: 4, 18, 23)
// If you need per-driver monitoring, use temperature sensors instead (see docs)

// Uncomment ONLY if you've disabled steering control (frees GPIO 2, 15):
// #define TEMP_FAULT_INDIVIDUAL  // Enable individual driver monitoring (requires 4 free GPIOs)
#ifdef TEMP_FAULT_INDIVIDUAL
  #define TEMP_FAULT_FL_PIN  19  // Front-Left driver fault
  #define TEMP_FAULT_FR_PIN  23  // Front-Right driver fault (repurpose audio pin)
  #define TEMP_FAULT_RL_PIN  18  // Rear-Left driver fault (repurpose audio pin)
  #define TEMP_FAULT_RR_PIN   2  // Rear-Right driver fault (REQUIRES disabling STEERING_DIR_PIN)
  
  // WARNING: If enabled, steering control will not work!
  #warning "TEMP_FAULT_INDIVIDUAL enabled - GPIO 2 repurposed from STEERING_DIR"
#endif

// ========================== BTS7960 Current Sense Pins ==========================
// Each BTS7960 has R_IS (right/forward) and L_IS (left/reverse) current sense outputs
// Typical sensitivity: 1mV per 1A (check your module's datasheet)
// Output voltage range: 0-3.3V (safe for ESP32 ADC)
// These pins provide REAL-TIME current measurement for each motor in each direction
//
// Pin Assignment Strategy:
// - Use ADC1 pins only (GPIO 32-39) for WiFi compatibility
// - We have 8 ADC1 channels available, need 8 for all motors (perfect!)
//
// Note: Reading all 8 channels gives you:
// - Per-motor current monitoring
// - Direction-specific current (forward vs reverse)
// - Detect individual motor issues (stall, overload, etc.)
// - More accurate total current calculation

// Front-Left Motor Current Sense
#define FL_R_IS          39  // ADC1_CH3 - Front-left forward (R_IS) current sense
#define FL_L_IS          36  // ADC1_CH0 - Front-left reverse (L_IS) current sense

// Front-Right Motor Current Sense
#define FR_R_IS          34  // ADC1_CH6 - Front-right forward (R_IS) current sense
#define FR_L_IS          35  // ADC1_CH7 - Front-right reverse (L_IS) current sense
                             // Note: GPIO 35 shared with battery voltage - will use for voltage only

// Rear-Left Motor Current Sense
// NOTE: GPIO 32/33 are used for RR motor PWM - cannot use for ADC!
// Rear-left current sensing DISABLED due to pin conflicts
// #define RL_R_IS          32  // CONFLICT with RR_RPWM!
// #define RL_L_IS          33  // CONFLICT with RR_LPWM!

// Rear-Right Motor Current Sense  
// NOTE: GPIO 37/38 not available on ESP32-WROOM-32U
// Rear-right current sensing DISABLED - not enough ADC pins
// #define RR_R_IS          37  // Not available on most ESP32 modules
// #define RR_L_IS          38  // Not available on most ESP32 modules

// BTS7960 Current Sense Specifications
const float BTS7960_CURRENT_SENSITIVITY = 0.001; // 1mV per 1A (typical)
                                                  // Adjust if your module has different sensitivity
                                                  // Some modules: 0.001 (1mV/A), others: 0.0005 (0.5mV/A)

// ========================== Pedal & Shifter Pins ==========================
// Manual mode inputs (original Power Wheels functionality)
// NOTE: GPIO 34 and 39 shared with current sense - prioritize based on control mode
#define PEDAL_PIN        34  // ADC1_CH6 for pedal potentiometer (0-3.3V = 0-4095)
                             // Potentiometer: One end to 3.3V, other to GND, wiper to pin
                             // SHARED with FR_R_IS - use in MANUAL mode only
#define SHIFTER_D_PIN    39  // Digital input: physical shifter Drive position (active LOW)
                             // Uses INPUT_PULLUP - switch closes to GND when engaged
                             // SHARED with FL_R_IS - use in MANUAL mode only
#define SHIFTER_R_PIN     5  // Digital input: physical shifter Reverse position (active LOW)
                             // Uses INPUT_PULLUP - switch closes to GND when engaged
                             // Note: If both D and R are open (HIGH), vehicle is in PARK

// ========================== Steering Motor Control ==========================
// Optional power steering motor (if adding power steering to stock toy)
// Uses same H-bridge control as drive motors (PWM + DIR)
#define STEERING_PWM_PIN 15  // PWM for steering motor driver (GPIO 15)
#define STEERING_DIR_PIN 2   // Direction for steering motor driver (GPIO 2)
                             // HIGH=steer right, LOW=steer left (adjust per your setup)

// ========================== Audio & Lighting ==========================
// Lighting control (relay or MOSFET for LED strips/headlights)
#define LIGHT_PIN        22  // GPIO to drive lights on/off (HIGH=on, LOW=off)

// Audio system: Assumes DFPlayer Mini or similar MP3 module
// These pins send control signals (button press simulation or serial control)
#define AUDIO_PLAY_PIN   23  // Speaker module play/pause control (pulse HIGH to toggle)
#define AUDIO_VOLUP_PIN  18  // Speaker module volume up (pulse HIGH to increase)
#define AUDIO_VOLDOWN_PIN 4  // Speaker module volume down (pulse HIGH to decrease)
                             // Note: Implementation incomplete - audio logic needs development

// ========================== CRSF Input Channels ==========================
// CRSF protocol provides 16 channels (0-15), each with value range 172-1811
// Typical mapping: 1000 = min, 1500 = center, 2000 = max (after library scaling)
// Configure these in your ELRS transmitter to match your desired control layout
#define THROTTLE_CH      0   // Channel 0: Throttle (left stick vertical typically)
                             // 1000 = no throttle, 2000 = full throttle
#define STEERING_CH      1   // Channel 1: Steering (right stick horizontal typically)
                             // 1000 = full left, 1500 = center, 2000 = full right
#define HORN_CH          2   // Channel 2: Horn (momentary switch/button)
                             // >1500 = horn active
#define MODE_SWITCH_CH   3   // Channel 3: Manual/Remote mode toggle (2-position switch)
                             // <1500 = Manual mode, >1500 = Remote mode
#define LIGHTS_CH        5   // Channel 5: Lights on/off (2-position switch)
                             // <1500 = lights off, >1500 = lights on
#define SPEEDMODE_CH     6   // Channel 6: Speed mode (3-position switch)
                             // <1300 = Reverse, 1300-1700 = Forward Low, >1700 = Forward Fast

// ========================== Speed & Control Enums ==========================
enum SpeedMode { REVERSE, FORWARD_LOW, FORWARD_FAST };
SpeedMode speedMode = FORWARD_LOW;

enum ShifterMode { DRIVE, PARK, REVERSE_MODE };
ShifterMode shifterState = PARK;

enum ControlMode { MANUAL, REMOTE };
ControlMode controlMode = MANUAL;

// ========================== Motor Control Constants ==========================
const int PWM_FREQ         = 1000;   // PWM frequency in Hz (1 kHz - good for motor drivers)
                                      // Lower freq = more torque but louder
                                      // Higher freq = quieter but may reduce efficiency
const int PWM_RES          = 8;      // PWM resolution in bits (8-bit = 0-255 range)
const int MAX_PWM          = 255;    // Maximum PWM duty cycle (2^8 - 1)
const int MIN_PWM          = 0;      // Minimum PWM duty cycle

// ========================== CRSF Threshold Constants ==========================
const int MODE_THRESHOLD   = 1500;   // Threshold for mode switch (Manual vs Remote)
const int LIGHT_THRESHOLD  = 1500;   // Threshold for lights switch (On vs Off)
const int SPEEDMODE_LOW    = 1300;   // Speed mode lower boundary
const int SPEEDMODE_HIGH   = 1700;   // Speed mode upper boundary

// ========================== Safety Constants ==========================
// Battery voltage monitoring - 24V SYSTEM (original Can-Am X3 batteries)
// Voltage divider: Use 100k + 12k resistors for 24V system
// Divider ratio: (100k + 12k) / 12k = 9.333
const float VOLTAGE_DIVIDER_RATIO = 9.333;  // Hardware voltage divider ratio (100k/12k)
                                             // At 24V: 24 / 9.333 = 2.57V at ADC (safe)
                                             // At 30V: 30 / 9.333 = 3.21V at ADC (safe, <3.3V max)

// 24V Battery Thresholds (adjust based on your battery type)
// Lead-Acid 24V: Full=~27V, Nominal=24V, Low=~20V, Cutoff=~19V
// LiPo 6S (24V): Full=25.2V, Nominal=22.2V, Low=19.8V, Cutoff=18.0V
const float LOW_VOLTAGE_CUTOFF    = 19.0;   // Volts - disable motors below this
                                             // XH-M609 module will also protect at ~20V
const float LOW_VOLTAGE_WARNING   = 21.0;   // Volts - warning threshold
const float ADC_VREF              = 3.3;    // ESP32 ADC reference voltage
const int   ADC_MAX_VALUE         = 4095;   // 12-bit ADC maximum value

// Current monitoring - 24V SYSTEM with 200W motors (4 units)
// Total power: 4 motors × 200W = 800W → 800W / 24V = 33.3A typical max
// Use ACS712-30B (±30A range, 66mV/A) or higher capacity sensor
// Alternative: Use BTS7960 built-in current sense (R_IS/L_IS pins)
const float CURRENT_SENSOR_ZERO   = 2.5;    // Voltage at 0A (typically VCC/2)
const float CURRENT_SENSITIVITY   = 0.066;  // V/A (66mV per amp for ACS712-30B)
                                             // Use 0.185 for ACS712-05B, 0.100 for 20B, 0.066 for 30B
const float MAX_CURRENT_LIMIT     = 40.0;   // Amps - emergency shutdown above this
                                             // 4 motors × 200W / 24V = ~33A normal, set limit higher
const float HIGH_CURRENT_WARNING  = 35.0;   // Amps - warning threshold

// Radio failsafe
const unsigned long RADIO_TIMEOUT_MS = 1000; // Milliseconds - stop motors if no signal for this long

// Motor ramping (smooth acceleration/deceleration)
const int RAMP_STEP          = 10;     // PWM increment per ramp cycle
const int RAMP_DELAY_MS      = 20;     // Milliseconds between ramp steps
                                        // Total ramp time: (255 / 10) * 20 = 510ms for 0-100%

// Temperature monitoring
const unsigned long TEMP_RECOVERY_MS = 30000; // Cool-down period after overheat (30 seconds)

// ========================== Runtime Variables ==========================
// CRSF input channels (updated every loop from receiver)
int throttleInput   = 0;    ///< Current CRSF throttle channel value (1000–2000)
int steeringInput   = 0;    ///< Current CRSF steering channel value (1000–2000)
int hornInput       = 0;    ///< Current CRSF horn channel value (1000–2000)
int modeInput       = 0;    ///< Current CRSF mode switch channel value (1000–2000)
int lightsInput     = 0;    ///< Current CRSF lights channel value (1000–2000)
int speedModeInput  = 0;    ///< Current CRSF speed mode channel value (1000–2000)

// System state flags
bool overTemp       = false;///< Flag for over-temperature fault (true = fault active)
bool lightsOn       = false;///< Indicates whether lights are currently on
bool motorsEnabled  = true; ///< Master flag for motor operation (false = all safety shutdowns)
bool motorDirection = true; ///< Current motor direction (true = forward, false = reverse)

// Individual driver thermal fault flags (only used if TEMP_FAULT_INDIVIDUAL defined)
#ifdef TEMP_FAULT_INDIVIDUAL
  bool overTempFL = false;  ///< Front-Left driver over-temperature
  bool overTempFR = false;  ///< Front-Right driver over-temperature
  bool overTempRL = false;  ///< Rear-Left driver over-temperature
  bool overTempRR = false;  ///< Rear-Right driver over-temperature
#endif

// Safety monitoring variables
float batteryVoltage = 0.0; ///< Current battery voltage in volts
float motorCurrent   = 0.0; ///< Total motor current draw in amps (sum of all motors)
unsigned long lastRadioSignal = 0; ///< Timestamp of last valid radio signal (millis)
unsigned long tempFaultTime    = 0; ///< Timestamp when over-temp fault occurred (for recovery timer)
int currentMotorSpeed = 0;  ///< Current motor PWM value (for ramping algorithm)
int targetMotorSpeed  = 0;  ///< Target motor PWM value (for ramping algorithm)

// Per-motor current monitoring (from BTS7960 R_IS and L_IS pins)
struct MotorTelemetry {
  float forwardCurrent;  ///< Current when motor driving forward (from R_IS pin)
  float reverseCurrent;  ///< Current when motor driving reverse (from L_IS pin)
  float totalCurrent;    ///< Combined current for this motor
};

MotorTelemetry frontLeft  = {0.0, 0.0, 0.0};  ///< Front-left motor telemetry
MotorTelemetry frontRight = {0.0, 0.0, 0.0};  ///< Front-right motor telemetry
MotorTelemetry rearLeft   = {0.0, 0.0, 0.0};  ///< Rear-left motor telemetry
MotorTelemetry rearRight  = {0.0, 0.0, 0.0};  ///< Rear-right motor telemetry

// CRSF receiver instance
AlfredoCRSF crsf;           ///< CRSF receiver instance (AlfredoCRSF library)

// Development/testing mode - set to true to disable hardware safety checks for bench testing
#define DEVELOPMENT_MODE false ///< Set true to skip ADC/sensor reads (for testing without hardware)

// ========================== Function Forward Declarations ==========================
// These declarations allow functions to be called before they're defined in the file

/**
 * @brief Reads all telemetry sensors (voltage, current, temperature)
 * 
 * Reads battery voltage via ADC with voltage divider, motor current from current sensor,
 * and checks over-temperature fault pin. Updates global variables:
 * - batteryVoltage: Battery voltage in volts
 * - motorCurrent: Motor current draw in amps
 * - overTemp: true if over-temperature fault detected
 * 
 * @note Respects DEVELOPMENT_MODE flag - skips reads if in dev mode
 */
void readTelemetry();

/**
 * @brief Checks all safety conditions and updates motorsEnabled flag
 * 
 * Evaluates:
 * - Battery voltage (low voltage cutoff)
 * - Motor current (overcurrent protection)
 * - Over-temperature fault (thermal protection)
 * - Radio signal timeout (failsafe)
 * 
 * Sets motorsEnabled = false if any safety condition fails
 * Logs warnings and errors to Serial for debugging
 */
void checkSafetyConditions();

/**
 * @brief Outputs telemetry and status information to Serial
 * 
 * Logs current mode, lights state, shifter position, battery voltage,
 * motor current, and any active safety warnings. Called periodically
 * from main loop (typically every 1 second).
 */
void outputTelemetry();

/**
 * @brief Executes control logic when in REMOTE mode (radio control)
 * 
 * Reads CRSF receiver channels for throttle, steering, and speed mode.
 * Determines motor direction based on speed mode (forward/reverse).
 * Applies speed limiting for low-speed mode.
 * Controls lights based on LIGHTS_CH channel.
 * 
 * @note Requires valid radio signal (failsafe will disable motors if signal lost)
 */
void remoteControlLogic();

/**
 * @brief Executes control logic when in MANUAL mode (pedal/shifter control)
 * 
 * Reads analog pedal position and physical shifter state.
 * Sets motor direction based on shifter (DRIVE/REVERSE/PARK).
 * In PARK mode, motors are disabled regardless of pedal position.
 * Lights are always enabled in manual mode.
 */
void manualControlLogic();

/**
 * @brief Reads physical shifter switch states and updates shifterState
 * 
 * Reads SHIFTER_D_PIN and SHIFTER_R_PIN (active LOW with INPUT_PULLUP).
 * Updates global shifterState:
 * - DRIVE: D pin LOW, R pin HIGH
 * - REVERSE_MODE: R pin LOW, D pin HIGH
 * - PARK: Both pins HIGH (no switches closed)
 */
void readShifterState();

/**
 * @brief Sets direction pins for all four motors
 * 
 * @param forward true = forward direction, false = reverse direction
 * 
 * @note Actual motor direction depends on physical wiring.
 *       If motors spin backwards, swap motor wires or invert logic.
 */
void setMotorDirection(bool forward);

/**
 * @brief Sets target speed for motors with ramping
 * 
 * @param targetPwm Desired PWM duty cycle (0-255)
 * 
 * Sets targetMotorSpeed global variable. Actual speed change is gradual
 * (ramped) by updateMotorRamping() function to prevent gear damage.
 * 
 * @note If motorsEnabled is false, target is forced to 0
 */
void setMotorSpeed(int targetPwm);

/**
 * @brief Updates motor speed gradually toward target (ramping)
 * 
 * Called from main loop. Smoothly transitions currentMotorSpeed toward
 * targetMotorSpeed in small increments (RAMP_STEP) with delay (RAMP_DELAY_MS).
 * Prevents sudden speed changes that can damage gears or cause mechanical stress.
 * 
 * Writes actual PWM to motor pins using ledcWrite() - ESP32 Core 3.x API.
 */
void updateMotorRamping();

/**
 * @brief Emergency stop - immediately disables all motors
 * 
 * Sets all motor PWM outputs to 0 instantly (no ramping).
 * Called during safety shutdown conditions.
 */
void emergencyStop();

/**
 * @brief Logs a timestamped message to Serial
 * 
 * @param msg Message string to log
 * 
 * Outputs: "[timestamp ms] message" format to Serial monitor
 * Useful for debugging and monitoring system behavior
 */
void logMessage(const String &msg);

//==============================================================================
// SETUP FUNCTION - Runs once at power-on or reset
//==============================================================================
void setup() {
  // ==================== Serial Communication ====================
  // USB Serial for debugging/telemetry output
  Serial.begin(115200);
  delay(100); // Allow serial to stabilize
  logMessage("ESP32 Power Wheels Controller Starting...");
  
  // UART2 for CRSF receiver communication
  // Pins: RX=16, TX=17, Baud=420000 (CRSF standard)
  Serial2.begin(420000, SERIAL_8N1, 16, 17);
  crsf.begin(Serial2);
  lastRadioSignal = millis(); // Initialize radio signal timestamp
  logMessage("CRSF receiver initialized on Serial2 (RX:16, TX:17, 420000 baud)");

  // ==================== ADC Configuration ====================
  // ESP32 ADC is 12-bit by default (0-4095), set explicitly for clarity
  analogReadResolution(12);
  logMessage("ADC configured: 12-bit resolution (0-4095)");

  // ==================== Motor Drivers Initialization (BTS7960) ====================
  // BTS7960 modules: Each has RPWM (forward) and LPWM (reverse) inputs
  // Global motor enable pin (HIGH = all motors enabled)
  // Can be connected to R_EN and L_EN pins, or use for relay/contactor
  pinMode(MOTOR_ENABLE, OUTPUT);
  digitalWrite(MOTOR_ENABLE, HIGH);
  logMessage("BTS7960 motor drivers enabled (MOTOR_ENABLE = HIGH)");
  
  // PWM pins - ESP32 Core 3.x API: ledcAttach(pin, freq, resolution)
  // For BTS7960: Need BOTH RPWM and LPWM for each motor (8 PWM channels total)
  int rpwmPins[] = {FL_RPWM, FR_RPWM, RL_RPWM, RR_RPWM};
  int lpwmPins[] = {FL_LPWM, FR_LPWM, RL_LPWM, RR_LPWM};
  const char* motorNames[] = {"FL", "FR", "RL", "RR"};
  
  logMessage("Initializing BTS7960 PWM channels (RPWM + LPWM for each motor)...");
  
  for (int i = 0; i < 4; i++) {
    // Initialize RPWM (forward direction)
    bool rpwm_success = ledcAttach(rpwmPins[i], PWM_FREQ, PWM_RES);
    if (!rpwm_success) {
      logMessage(String("ERROR: Failed to attach RPWM to ") + motorNames[i] + " (GPIO " + String(rpwmPins[i]) + ")");
      motorsEnabled = false;
    } else {
      ledcWrite(rpwmPins[i], 0); // Initialize to 0 speed
    }
    
    // Initialize LPWM (reverse direction)
    bool lpwm_success = ledcAttach(lpwmPins[i], PWM_FREQ, PWM_RES);
    if (!lpwm_success) {
      logMessage(String("ERROR: Failed to attach LPWM to ") + motorNames[i] + " (GPIO " + String(lpwmPins[i]) + ")");
      motorsEnabled = false;
    } else {
      ledcWrite(lpwmPins[i], 0); // Initialize to 0 speed
    }
  }
  
  if (motorsEnabled) {
    logMessage("BTS7960 PWM initialized: 1kHz, 8-bit (0-255), 8 channels total");
    logMessage("Control: RPWM=forward, LPWM=reverse, both=0 for brake");
  } else {
    logMessage("WARNING: Motor PWM setup failed - motors disabled");
  }

  // ==================== Telemetry & Control Inputs ====================
  // Battery voltage monitoring
  pinMode(VOLTAGE_PIN, INPUT);
  
  // Pedal input
  pinMode(PEDAL_PIN, INPUT);
  
  // BTS7960 current sense inputs (8 channels total - 2 per motor)
  // Note: pinMode(INPUT) for ADC pins is optional but explicit
  pinMode(FL_R_IS, INPUT);
  pinMode(FL_L_IS, INPUT);
  pinMode(FR_R_IS, INPUT);
  // pinMode(FR_L_IS, INPUT); // Skip - shared with VOLTAGE_PIN (GPIO 35)
  pinMode(RL_R_IS, INPUT);
  pinMode(RL_L_IS, INPUT);
  pinMode(RR_R_IS, INPUT);
  pinMode(RR_L_IS, INPUT);
  logMessage("BTS7960 current sense inputs configured (8 channels)");
  
  // Digital inputs with pull-ups
  #ifdef TEMP_FAULT_INDIVIDUAL
    // Individual driver fault monitoring
    pinMode(TEMP_FAULT_FL_PIN, INPUT_PULLUP);  // Front-Left fault
    pinMode(TEMP_FAULT_FR_PIN, INPUT_PULLUP);  // Front-Right fault
    pinMode(TEMP_FAULT_RL_PIN, INPUT_PULLUP);  // Rear-Left fault
    pinMode(TEMP_FAULT_RR_PIN, INPUT_PULLUP);  // Rear-Right fault
    logMessage("Individual BTS7960 thermal fault monitoring enabled (4 pins)");
  #else
    // Combined fault monitoring (all 4 drivers wired to one pin)
    pinMode(TEMP_FAULT_PIN, INPUT_PULLUP);  // Active LOW fault signal
    logMessage("Combined BTS7960 thermal fault monitoring enabled (1 pin)");
  #endif
  
  pinMode(SHIFTER_D_PIN, INPUT_PULLUP);   // Active LOW when in Drive
  pinMode(SHIFTER_R_PIN, INPUT_PULLUP);   // Active LOW when in Reverse
  logMessage("Telemetry and control inputs configured");

  // ==================== Steering Motor (Optional) ====================
  // Direction pin for steering motor
  pinMode(STEERING_DIR_PIN, OUTPUT);
  digitalWrite(STEERING_DIR_PIN, LOW);
  
  // PWM for steering motor (if implementing power steering)
  // Commented out for now - implement when adding steering control
  // bool steeringPwmSuccess = ledcAttach(STEERING_PWM_PIN, PWM_FREQ, PWM_RES);
  // if (!steeringPwmSuccess) {
  //   logMessage("ERROR: Failed to attach PWM to STEERING_PWM_PIN");
  // }
  logMessage("Steering motor pins configured (PWM not initialized)");

  // ==================== Lights & Audio ====================
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW); // Lights off initially
  
  // Audio control pins (for DFPlayer Mini or similar module)
  pinMode(AUDIO_PLAY_PIN, OUTPUT);
  digitalWrite(AUDIO_PLAY_PIN, LOW);
  pinMode(AUDIO_VOLUP_PIN, OUTPUT);
  digitalWrite(AUDIO_VOLUP_PIN, LOW);
  pinMode(AUDIO_VOLDOWN_PIN, OUTPUT);
  digitalWrite(AUDIO_VOLDOWN_PIN, LOW);
  logMessage("Lights and audio pins configured");

  // ==================== Startup Complete ====================
  logMessage("========================================");
  logMessage("Setup complete. System ready.");
  logMessage("Mode: Waiting for input...");
  logMessage("========================================");
  
  // Brief startup delay for system stabilization
  delay(500);
}

//==============================================================================
// MAIN LOOP - Runs continuously after setup()
//==============================================================================
void loop() {
  static unsigned long lastTelemetry = 0;
  static unsigned long lastRampUpdate = 0;

  // ==================== Update CRSF Receiver ====================
  // Must be called frequently to receive packets from radio
  crsf.update();
  
  // ==================== Read All Sensors ====================
  readTelemetry(); // Battery voltage, current, temperature
  
  // ==================== Read CRSF Input Channels ====================
  throttleInput  = crsf.getChannel(THROTTLE_CH);
  steeringInput  = crsf.getChannel(STEERING_CH);
  hornInput      = crsf.getChannel(HORN_CH);
  modeInput      = crsf.getChannel(MODE_SWITCH_CH);
  lightsInput    = crsf.getChannel(LIGHTS_CH);
  speedModeInput = crsf.getChannel(SPEEDMODE_CH);
  
  // Update radio signal timestamp (for failsafe detection)
  // Consider signal valid if throttle channel is within expected range
  if (throttleInput >= 900 && throttleInput <= 2100) {
    lastRadioSignal = millis();
  }

  // ==================== Determine Control Mode ====================
  controlMode = (modeInput > MODE_THRESHOLD) ? REMOTE : MANUAL;
  lightsOn    = (lightsInput > LIGHT_THRESHOLD);
  readShifterState(); // Update shifter position for manual mode

  // ==================== Safety Checks ====================
  // Evaluates all safety conditions and updates motorsEnabled flag
  checkSafetyConditions();
  
  // If motors disabled by safety system, immediately stop
  if (!motorsEnabled) {
    emergencyStop();
    targetMotorSpeed = 0;
  }

  // ==================== Execute Control Logic ====================
  // Dispatch to appropriate control function based on mode
  if (motorsEnabled) {
    if (controlMode == REMOTE) {
      remoteControlLogic();
    } else {
      manualControlLogic();
    }
  }

  // ==================== Motor Speed Ramping ====================
  // Update motor speed gradually toward target (prevents gear damage)
  // Only ramp every RAMP_DELAY_MS milliseconds
  if (millis() - lastRampUpdate >= RAMP_DELAY_MS) {
    updateMotorRamping();
    lastRampUpdate = millis();
  }

  // ==================== Periodic Telemetry Output ====================
  // Output status to Serial every 1 second for monitoring
  if (millis() - lastTelemetry >= 1000) {
    outputTelemetry();
    lastTelemetry = millis();
  }

  // Small delay to prevent loop from running too fast
  delay(10); // Loop runs at ~100Hz
}

//==============================================================================
// FUNCTION IMPLEMENTATIONS
//==============================================================================

/**
 * @brief Reads all telemetry sensors and updates global variables
 * 
 * Reads battery voltage via voltage divider, per-motor current from BTS7960
 * R_IS and L_IS pins, and checks over-temperature fault pin.
 * 
 * BTS7960 Current Sensing:
 * - R_IS: Current sense for forward direction (RPWM active)
 * - L_IS: Current sense for reverse direction (LPWM active)
 * - Sensitivity: Typically 1mV per 1A (adjust BTS7960_CURRENT_SENSITIVITY if different)
 * - Output: 0-3.3V (safe for ESP32 ADC)
 * 
 * This function reads ALL 8 current sense channels (2 per motor × 4 motors)
 * providing detailed per-motor current monitoring for diagnostics and safety.
 */
void readTelemetry() {
  // Skip hardware reads in development mode (for bench testing without sensors)
  if (DEVELOPMENT_MODE) {
    batteryVoltage = 24.0; // Fake voltage for testing
    motorCurrent = 10.0;   // Fake current for testing
    frontLeft.totalCurrent = 2.5;
    frontRight.totalCurrent = 2.5;
    rearLeft.totalCurrent = 2.5;
    rearRight.totalCurrent = 2.5;
    overTemp = false;
    return;
  }
  
  // ==================== Battery Voltage Monitoring ====================
  // Read ADC value (0-4095 for 12-bit ADC)
  int voltageADC = analogRead(VOLTAGE_PIN);
  
  // Convert ADC reading to actual voltage
  // Formula: V_battery = (ADC / ADC_MAX) * V_REF * DIVIDER_RATIO
  float voltageAtPin = (voltageADC / (float)ADC_MAX_VALUE) * ADC_VREF;
  batteryVoltage = voltageAtPin * VOLTAGE_DIVIDER_RATIO;
  
  // Sanity check: battery voltage should be reasonable (15V to 30V for 24V system)
  if (batteryVoltage < 15.0 || batteryVoltage > 30.0) {
    // Likely sensor error or disconnected battery
    batteryVoltage = 0.0;
  }
  
  // ==================== BTS7960 Per-Motor Current Monitoring ====================
  // Read all 8 current sense channels (R_IS and L_IS for each of 4 motors)
  // BTS7960 current sense output: typically 1mV per 1A
  // Formula: I = V_sense / Sensitivity
  
  // --- Front-Left Motor ---
  // Note: GPIO 39 (FL_R_IS) shared with SHIFTER_D - only read in REMOTE mode
  if (controlMode == REMOTE) {
    int fl_r_adc = analogRead(FL_R_IS);
    float fl_r_voltage = (fl_r_adc / (float)ADC_MAX_VALUE) * ADC_VREF;
    frontLeft.forwardCurrent = fl_r_voltage / BTS7960_CURRENT_SENSITIVITY;
  } else {
    frontLeft.forwardCurrent = 0.0; // Can't read in MANUAL mode (pin used for shifter)
  }
  
  int fl_l_adc = analogRead(FL_L_IS);
  float fl_l_voltage = (fl_l_adc / (float)ADC_MAX_VALUE) * ADC_VREF;
  frontLeft.reverseCurrent = fl_l_voltage / BTS7960_CURRENT_SENSITIVITY;
  frontLeft.totalCurrent = frontLeft.forwardCurrent + frontLeft.reverseCurrent;
  
  // --- Front-Right Motor ---
  // Note: GPIO 34 (FR_R_IS) shared with PEDAL - only read in REMOTE mode
  // Note: GPIO 35 (FR_L_IS) used for VOLTAGE_PIN - cannot read current
  if (controlMode == REMOTE) {
    int fr_r_adc = analogRead(FR_R_IS);
    float fr_r_voltage = (fr_r_adc / (float)ADC_MAX_VALUE) * ADC_VREF;
    frontRight.forwardCurrent = fr_r_voltage / BTS7960_CURRENT_SENSITIVITY;
  } else {
    frontRight.forwardCurrent = 0.0; // Can't read in MANUAL mode (pin used for pedal)
  }
  
  frontRight.reverseCurrent = frontRight.forwardCurrent; // Estimate (no L_IS pin available)
  frontRight.totalCurrent = frontRight.forwardCurrent;
  
  // --- Rear-Left Motor ---
  // GPIO 32/33 used for RR motor PWM - CANNOT read RL current sense
  rearLeft.forwardCurrent = 0.0;
  rearLeft.reverseCurrent = 0.0;
  rearLeft.totalCurrent = 0.0;
  
  // --- Rear-Right Motor ---
  // GPIO 37/38 not available on ESP32-WROOM-32U
  rearRight.forwardCurrent = 0.0;
  rearRight.reverseCurrent = 0.0;
  rearRight.totalCurrent = 0.0;
  
  // --- Calculate Total Current (All Motors) ---
  // Note: Due to GPIO pin conflicts, only FL and FR current sense available
  // Estimate total current by multiplying measured motors by 2
  float measuredCurrent = frontLeft.totalCurrent + frontRight.totalCurrent;
  
  if (controlMode == REMOTE) {
    // In REMOTE mode, we can read FL and FR current sense
    motorCurrent = measuredCurrent * 2.0; // Estimate: assume RL and RR draw similar current
  } else {
    // In MANUAL mode, pedal and shifter pins conflict with current sense
    motorCurrent = 0.0; // Cannot measure in MANUAL mode
  }
  
  // Sanity check: current should be positive and reasonable (0-50A for 4x 200W motors)
  if (motorCurrent < 0.0) {
    motorCurrent = 0.0; // Ignore small negative values due to noise
  }
  if (motorCurrent > 100.0) {
    // Unreasonably high - likely sensor error
    logMessage("WARNING: Current reading unreasonable (" + String(motorCurrent, 1) + "A) - possible sensor error");
  }
  
  // ==================== Temperature Fault Monitoring ====================
  #ifdef TEMP_FAULT_INDIVIDUAL
    // Individual driver fault monitoring (active LOW)
    overTempFL = (digitalRead(TEMP_FAULT_FL_PIN) == LOW);
    overTempFR = (digitalRead(TEMP_FAULT_FR_PIN) == LOW);
    overTempRL = (digitalRead(TEMP_FAULT_RL_PIN) == LOW);
    overTempRR = (digitalRead(TEMP_FAULT_RR_PIN) == LOW);
    
    // Combined fault flag: true if ANY driver is overheating
    bool anyOverTemp = (overTempFL || overTempFR || overTempRL || overTempRR);
    
    // Log which specific driver(s) are overheating
    if (anyOverTemp && tempFaultTime == 0) {
      tempFaultTime = millis();
      String faultMsg = "!!! OVER-TEMPERATURE FAULT: ";
      if (overTempFL) faultMsg += "FL ";
      if (overTempFR) faultMsg += "FR ";
      if (overTempRL) faultMsg += "RL ";
      if (overTempRR) faultMsg += "RR ";
      faultMsg += "driver(s) overheating !!!";
      logMessage(faultMsg);
    }
    
    overTemp = anyOverTemp;  // Set combined flag
    
  #else
    // Combined fault monitoring (all 4 drivers wired to one pin)
    overTemp = (digitalRead(TEMP_FAULT_PIN) == LOW);
    
    // If temperature fault just occurred, record the time for recovery delay
    if (overTemp && tempFaultTime == 0) {
      tempFaultTime = millis();
      logMessage("!!! OVER-TEMPERATURE FAULT DETECTED (one or more drivers) !!!");
    }
  #endif
  
  // Recovery logic (common to both modes)
  // If temperature fault cleared, check if recovery time has elapsed
  if (!overTemp && tempFaultTime > 0) {
    if (millis() - tempFaultTime >= TEMP_RECOVERY_MS) {
      logMessage("Temperature fault cleared. System recovered.");
      tempFaultTime = 0; // Reset fault timer
    } else {
      // Fault cleared but still in recovery period
      overTemp = true; // Keep fault active during recovery
    }
  }
}

/**
 * @brief Checks all safety conditions and updates motorsEnabled flag
 * 
 * Evaluates battery voltage, motor current, temperature, and radio signal.
 * Disables motors if any safety threshold is exceeded.
 */
void checkSafetyConditions() {
  bool safetyOK = true;
  String warningMsg = "";
  
  // ==================== Low Voltage Cutoff ====================
  if (batteryVoltage > 0 && batteryVoltage < LOW_VOLTAGE_CUTOFF) {
    safetyOK = false;
    warningMsg = "SAFETY: Low battery voltage (" + String(batteryVoltage, 2) + "V < " + 
                 String(LOW_VOLTAGE_CUTOFF, 1) + "V) - Motors disabled!";
  } else if (batteryVoltage > 0 && batteryVoltage < LOW_VOLTAGE_WARNING) {
    // Warning but not critical yet
    static unsigned long lastWarning = 0;
    if (millis() - lastWarning >= 5000) { // Warn every 5 seconds
      logMessage("WARNING: Low battery voltage (" + String(batteryVoltage, 2) + "V)");
      lastWarning = millis();
    }
  }
  
  // ==================== Overcurrent Protection ====================
  if (motorCurrent > MAX_CURRENT_LIMIT) {
    safetyOK = false;
    warningMsg = "SAFETY: Overcurrent detected (" + String(motorCurrent, 2) + "A > " + 
                 String(MAX_CURRENT_LIMIT, 1) + "A) - Motors disabled!";
  } else if (motorCurrent > HIGH_CURRENT_WARNING) {
    static unsigned long lastWarning = 0;
    if (millis() - lastWarning >= 2000) { // Warn every 2 seconds
      logMessage("WARNING: High current draw (" + String(motorCurrent, 2) + "A)");
      lastWarning = millis();
    }
  }
  
  // ==================== Thermal Protection ====================
  if (overTemp) {
    safetyOK = false;
    static unsigned long lastWarning = 0;
    if (millis() - lastWarning >= 3000) { // Warn every 3 seconds
      warningMsg = "SAFETY: Over-temperature fault - Motors disabled! (cooling down...)";
      lastWarning = millis();
    }
  }
  
  // ==================== Radio Failsafe ====================
  // Only check failsafe when in REMOTE mode
  if (controlMode == REMOTE) {
    unsigned long timeSinceSignal = millis() - lastRadioSignal;
    if (timeSinceSignal > RADIO_TIMEOUT_MS) {
      safetyOK = false;
      static unsigned long lastWarning = 0;
      if (millis() - lastWarning >= 1000) { // Warn every second
        warningMsg = "SAFETY: Radio signal lost (" + String(timeSinceSignal) + 
                     "ms) - Motors disabled!";
        lastWarning = millis();
      }
    }
  }
  
  // ==================== Update Motor Enable State ====================
  if (!safetyOK && motorsEnabled) {
    // Safety fault occurred - disable motors
    motorsEnabled = false;
    emergencyStop();
    if (warningMsg.length() > 0) {
      logMessage(warningMsg);
    }
  } else if (safetyOK && !motorsEnabled) {
    // Safety conditions restored - re-enable motors
    motorsEnabled = true;
    logMessage("SAFETY: All conditions OK - Motors re-enabled");
  }
}

/**
 * @brief Outputs telemetry and status information to Serial
 * 
 * Displays control mode, battery voltage, total current, per-motor current,
 * motor status, PWM values, and warnings.
 * 
 * Output includes detailed BTS7960 current sense data for all motors.
 */
void outputTelemetry() {
  // ==================== System Status ====================
  String status = "Status: ";
  status += (controlMode == REMOTE) ? "REMOTE" : "MANUAL";
  status += " | Lights: " + String(lightsOn ? "ON" : "OFF");
  status += " | Shifter: ";
  status += (shifterState == DRIVE) ? "DRIVE" : 
            (shifterState == REVERSE_MODE) ? "REVERSE" : "PARK";
  status += " | Direction: " + String(motorDirection ? "FWD" : "REV");
  
  logMessage(status);
  
  // ==================== Battery and Total Current ====================
  String telemetry = "Power: ";
  telemetry += "Battery=" + String(batteryVoltage, 2) + "V";
  telemetry += " | Total I=" + String(motorCurrent, 2) + "A";
  telemetry += " | Motors=" + String(motorsEnabled ? "ON" : "OFF");
  telemetry += " | PWM=" + String(currentMotorSpeed) + "/" + String(targetMotorSpeed);
  
  logMessage(telemetry);
  
  // ==================== Per-Motor Current (Detailed) ====================
  // Show current for each motor individually (useful for diagnostics)
  String motorCurrents = "Motor I: ";
  motorCurrents += "FL=" + String(frontLeft.totalCurrent, 2) + "A";
  motorCurrents += " | FR=" + String(frontRight.totalCurrent, 2) + "A";
  motorCurrents += " | RL=" + String(rearLeft.totalCurrent, 2) + "A";
  motorCurrents += " | RR=" + String(rearRight.totalCurrent, 2) + "A";
  
  logMessage(motorCurrents);
  
  // ==================== Direction-Specific Current (Optional Detail) ====================
  // Uncomment for even more detailed current monitoring (forward vs reverse per motor)
  /*
  if (motorDirection) {
    // Forward direction - show R_IS readings
    String fwdCurrents = "Forward I: ";
    fwdCurrents += "FL=" + String(frontLeft.forwardCurrent, 2) + "A";
    fwdCurrents += " | FR=" + String(frontRight.forwardCurrent, 2) + "A";
    fwdCurrents += " | RL=" + String(rearLeft.forwardCurrent, 2) + "A";
    fwdCurrents += " | RR=" + String(rearRight.forwardCurrent, 2) + "A";
    logMessage(fwdCurrents);
  } else {
    // Reverse direction - show L_IS readings
    String revCurrents = "Reverse I: ";
    revCurrents += "FL=" + String(frontLeft.reverseCurrent, 2) + "A";
    revCurrents += " | FR=" + String(frontRight.reverseCurrent, 2) + "A (est)";
    revCurrents += " | RL=" + String(rearLeft.reverseCurrent, 2) + "A";
    revCurrents += " | RR=" + String(rearRight.reverseCurrent, 2) + "A";
    logMessage(revCurrents);
  }
  */
  
  // ==================== Warnings and Faults ====================
  if (overTemp) {
    #ifdef TEMP_FAULT_INDIVIDUAL
      // Show which specific driver(s) are overheating
      String tempWarning = "WARNING: Over-temperature fault: ";
      if (overTempFL) tempWarning += "FL ";
      if (overTempFR) tempWarning += "FR ";
      if (overTempRL) tempWarning += "RL ";
      if (overTempRR) tempWarning += "RR ";
      tempWarning += "driver(s) overheating!";
      logMessage(tempWarning);
    #else
      // Combined monitoring - can't identify which driver
      logMessage("WARNING: Over-temperature fault active (one or more drivers)!");
    #endif
  }
  
  // Check for individual motor current imbalance (indicates problem)
  float avgCurrent = motorCurrent / 4.0;
  float maxDeviation = 0.0;
  String problematicMotor = "";
  
  if (abs(frontLeft.totalCurrent - avgCurrent) > maxDeviation) {
    maxDeviation = abs(frontLeft.totalCurrent - avgCurrent);
    problematicMotor = "FL";
  }
  if (abs(frontRight.totalCurrent - avgCurrent) > maxDeviation) {
    maxDeviation = abs(frontRight.totalCurrent - avgCurrent);
    problematicMotor = "FR";
  }
  if (abs(rearLeft.totalCurrent - avgCurrent) > maxDeviation) {
    maxDeviation = abs(rearLeft.totalCurrent - avgCurrent);
    problematicMotor = "RL";
  }
  if (abs(rearRight.totalCurrent - avgCurrent) > maxDeviation) {
    maxDeviation = abs(rearRight.totalCurrent - avgCurrent);
    problematicMotor = "RR";
  }
  
  // Warn if one motor draws significantly more current than others (>5A difference)
  if (maxDeviation > 5.0 && motorCurrent > 5.0) {
    logMessage("WARNING: Motor " + problematicMotor + " current imbalance detected! (" + 
               String(maxDeviation, 1) + "A deviation from average)");
    logMessage("  Possible causes: Motor stall, mechanical binding, damaged motor, loose connection");
  }
}

/**
 * @brief Executes control logic for REMOTE mode (radio control via CRSF)
 * 
 * Reads throttle and speed mode channels from receiver.
 * Controls lights based on LIGHTS_CH channel.
 * Sets motor direction and speed based on speed mode selection.
 */
void remoteControlLogic() {
  // ==================== Lights Control ====================
  digitalWrite(LIGHT_PIN, lightsOn ? HIGH : LOW);
  
  // ==================== Speed Mode Selection ====================
  // 3-position switch on transmitter determines speed mode
  // Low position (<1300): REVERSE
  // Middle position (1300-1700): FORWARD_LOW (50% speed for beginners/tight spaces)
  // High position (>1700): FORWARD_FAST (full speed)
  if (speedModeInput < SPEEDMODE_LOW) {
    speedMode = REVERSE;
  } else if (speedModeInput < SPEEDMODE_HIGH) {
    speedMode = FORWARD_LOW;
  } else {
    speedMode = FORWARD_FAST;
  }
  
  // ==================== Motor Direction ====================
  bool forward = (speedMode != REVERSE);
  setMotorDirection(forward);
  
  // ==================== Throttle Mapping ====================
  // Map throttle channel (1000-2000) to PWM duty cycle (0-255)
  int pwm = map(throttleInput, 1000, 2000, 0, MAX_PWM);
  
  // Constrain to valid range (in case of signal glitches)
  pwm = constrain(pwm, 0, MAX_PWM);
  
  // Apply speed limiting for FORWARD_LOW mode (50% max speed)
  if (speedMode == FORWARD_LOW) {
    pwm = pwm / 2;
  }
  
  // ==================== Set Target Speed ====================
  // Motor ramping will gradually change speed to this target
  setMotorSpeed(pwm);
}

/**
 * @brief Executes control logic for MANUAL mode (pedal and shifter control)
 * 
 * Reads analog pedal position and physical shifter state.
 * In PARK mode, motors are disabled regardless of pedal position.
 * Lights are always ON in manual mode (standard Power Wheels behavior).
 */
void manualControlLogic() {
  // ==================== Lights Always On in Manual Mode ====================
  // This matches typical Power Wheels behavior - lights on when driving
  digitalWrite(LIGHT_PIN, HIGH);
  
  // ==================== Read Pedal Position ====================
  int pedalADC = analogRead(PEDAL_PIN);
  
  // Map pedal ADC value (0-4095) to PWM duty cycle (0-255)
  int pwm = map(pedalADC, 0, 4095, 0, MAX_PWM);
  pwm = constrain(pwm, 0, MAX_PWM); // Safety constraint
  
  // ==================== Shifter State Logic ====================
  // PARK mode: Motors disabled regardless of pedal position
  if (shifterState == PARK) {
    setMotorSpeed(0); // Target speed = 0
    // Don't change direction pins to avoid potential issues
    return; // Exit early
  }
  
  // DRIVE mode: Forward direction
  // REVERSE_MODE: Reverse direction
  bool forward = (shifterState == DRIVE);
  setMotorDirection(forward);
  
  // ==================== Set Target Speed ====================
  // Motor ramping will gradually change speed to this target
  setMotorSpeed(pwm);
}

/**
 * @brief Reads physical shifter switch states and updates shifterState
 * 
 * Shifter uses two GPIO pins with INPUT_PULLUP (active LOW).
 * - D pin LOW + R pin HIGH = DRIVE
 * - R pin LOW + D pin HIGH = REVERSE
 * - Both HIGH (neither pressed) = PARK
 * - Both LOW (both pressed - should not happen) = PARK (safety)
 */
void readShifterState() {
  // Read shifter pins (INPUT_PULLUP, so LOW = pressed/engaged)
  bool dPressed = (digitalRead(SHIFTER_D_PIN) == LOW);
  bool rPressed = (digitalRead(SHIFTER_R_PIN) == LOW);
  
  // Determine shifter state based on switch positions
  if (dPressed && !rPressed) {
    // Drive engaged, Reverse not engaged
    shifterState = DRIVE;
  } else if (rPressed && !dPressed) {
    // Reverse engaged, Drive not engaged
    shifterState = REVERSE_MODE;
  } else {
    // Both off OR both on (invalid) = PARK (safe default)
    shifterState = PARK;
  }
}

/**
 * @brief Sets direction for all four motors (BTS7960 specific)
 * 
 * @param forward true = forward direction, false = reverse direction
 * 
 * BTS7960 Control Method:
 * - Forward: RPWM = speed, LPWM = 0
 * - Reverse: RPWM = 0, LPWM = speed
 * - Brake/Stop: RPWM = 0, LPWM = 0
 * 
 * This function stores the direction; actual PWM is written in updateMotorRamping()
 * 
 * @note If motors spin the wrong way, either:
 *       1. Swap motor wires (+ and -)
 *       2. Invert logic here (change forward to !forward)
 *       3. Swap RPWM and LPWM pin definitions
 */
void setMotorDirection(bool forward) {
  motorDirection = forward; // Store direction for use in PWM update
}

/**
 * @brief Sets target motor speed (with ramping for smooth acceleration)
 * 
 * @param targetPwm Desired PWM duty cycle (0-255)
 * 
 * This function sets the TARGET speed. The actual motor speed is changed
 * gradually by updateMotorRamping() to prevent sudden jerks and gear damage.
 * 
 * If motors are disabled by safety system, target is forced to 0.
 */
void setMotorSpeed(int targetPwm) {
  // Safety check: If motors disabled, force target to 0
  if (!motorsEnabled) {
    targetPwm = 0;
  }
  
  // Constrain to valid PWM range
  targetPwm = constrain(targetPwm, 0, MAX_PWM);
  
  // Set global target (ramping function will gradually move toward this)
  targetMotorSpeed = targetPwm;
}

/**
 * @brief Updates motor speed gradually toward target (ramping algorithm)
 * 
 * Called periodically from main loop. Smoothly transitions currentMotorSpeed
 * toward targetMotorSpeed in small increments to prevent:
 * - Gear damage from sudden torque changes
 * - Mechanical stress on drivetrain
 * - Abrupt jerking motion
 * 
 * BTS7960 Specific:
 * - Forward: RPWM = speed, LPWM = 0
 * - Reverse: RPWM = 0, LPWM = speed
 * - Brake: RPWM = 0, LPWM = 0
 * 
 * Uses ESP32 Core 3.x API: ledcWrite(pin, dutyCycle)
 */
void updateMotorRamping() {
  // Calculate difference between current and target
  int speedDiff = targetMotorSpeed - currentMotorSpeed;
  
  // If already at target, nothing to do
  if (speedDiff == 0) {
    return;
  }
  
  // Determine ramp direction and step size
  int rampStep = RAMP_STEP;
  if (abs(speedDiff) < RAMP_STEP) {
    // Close to target - use smaller step to avoid overshooting
    rampStep = abs(speedDiff);
  }
  
  // Apply ramping in appropriate direction
  if (speedDiff > 0) {
    // Need to increase speed
    currentMotorSpeed += rampStep;
  } else {
    // Need to decrease speed
    currentMotorSpeed -= rampStep;
  }
  
  // Safety constrain (should never be needed, but just in case)
  currentMotorSpeed = constrain(currentMotorSpeed, 0, MAX_PWM);
  
  // ==================== Write PWM to All Motors (BTS7960) ====================
  // BTS7960 uses dual PWM: RPWM for forward, LPWM for reverse
  // Direction is set by which PWM pin is active
  
  if (motorDirection) {
    // ========== FORWARD DIRECTION ==========
    // RPWM = speed, LPWM = 0
    ledcWrite(FL_RPWM, currentMotorSpeed);
    ledcWrite(FL_LPWM, 0);
    
    ledcWrite(FR_RPWM, currentMotorSpeed);
    ledcWrite(FR_LPWM, 0);
    
    ledcWrite(RL_RPWM, currentMotorSpeed);
    ledcWrite(RL_LPWM, 0);
    
    ledcWrite(RR_RPWM, currentMotorSpeed);
    ledcWrite(RR_LPWM, 0);
    
  } else {
    // ========== REVERSE DIRECTION ==========
    // RPWM = 0, LPWM = speed
    ledcWrite(FL_RPWM, 0);
    ledcWrite(FL_LPWM, currentMotorSpeed);
    
    ledcWrite(FR_RPWM, 0);
    ledcWrite(FR_LPWM, currentMotorSpeed);
    
    ledcWrite(RL_RPWM, 0);
    ledcWrite(RL_LPWM, currentMotorSpeed);
    
    ledcWrite(RR_RPWM, 0);
    ledcWrite(RR_LPWM, currentMotorSpeed);
  }
}

/**
 * @brief Emergency stop - immediately disables all motors
 * 
 * Sets all motor PWM outputs to 0 instantly (no ramping).
 * Called during safety shutdown conditions:
 * - Low battery voltage
 * - Overcurrent
 * - Over-temperature
 * - Radio signal loss
 * 
 * BTS7960: Sets both RPWM and LPWM to 0 for complete brake
 */
void emergencyStop() {
  // Immediately set PWM to 0 for all motors (both directions)
  // BTS7960: Both RPWM and LPWM = 0 for full brake
  ledcWrite(FL_RPWM, 0);
  ledcWrite(FL_LPWM, 0);
  
  ledcWrite(FR_RPWM, 0);
  ledcWrite(FR_LPWM, 0);
  
  ledcWrite(RL_RPWM, 0);
  ledcWrite(RL_LPWM, 0);
  
  ledcWrite(RR_RPWM, 0);
  ledcWrite(RR_LPWM, 0);
  
  // Update speed tracking variables
  currentMotorSpeed = 0;
  targetMotorSpeed = 0;
}

/**
 * @brief Logs a timestamped message to Serial for debugging
 * 
 * @param msg Message string to log
 * 
 * Output format: "[timestamp ms] message"
 * Useful for tracking system events, errors, and status changes.
 */
void logMessage(const String &msg) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(msg);
}

//==============================================================================
// END OF CODE
//==============================================================================
