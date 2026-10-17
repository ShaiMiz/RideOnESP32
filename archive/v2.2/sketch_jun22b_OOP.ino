/*******************************************************************************
 * ESP32 Power Wheels Controller Firmware - Can-Am Maverick X3 (OOP Refactored)
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
 * - 4-wheel independent motor control with BTS7960 drivers (OOP design)
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
 * Version: 2.2 (OOP Refactored)
 * Last Updated: December 2025
 ******************************************************************************/

#include <AlfredoCRSF.h>    // For CRSF receiver (ExpressLRS ELRS Nano 2.4GHz)

// ========================== I/O Expander Support ==========================
// Uncomment to enable MCP23017 I/O expander (adds 16 GPIO pins via I2C)
// #define USE_MCP23017_EXPANDER
#ifdef USE_MCP23017_EXPANDER
  #include <Wire.h>
  #include <Adafruit_MCP23X17.h>
  
  // I2C pins for MCP23017 (using free audio pins)
  #define I2C_SDA_PIN  23  // Was: AUDIO_PLAY_PIN (repurposed)
  #define I2C_SCL_PIN  18  // Was: AUDIO_VOLUP_PIN (repurposed)
  
  // MCP23017 pin assignments (0-15)
  #define MCP_TEMP_FL   0  // GPA0: Front-Left thermal fault
  #define MCP_TEMP_FR   1  // GPA1: Front-Right thermal fault
  #define MCP_TEMP_RL   2  // GPA2: Rear-Left thermal fault
  #define MCP_TEMP_RR   3  // GPA3: Rear-Right thermal fault
  #define MCP_AUDIO_PLAY    4  // GPA4: Audio play (future)
  #define MCP_AUDIO_VOLUP   5  // GPA5: Audio vol+ (future)
  #define MCP_AUDIO_VOLDOWN 6  // GPA6: Audio vol- (future)
  #define MCP_SPARE_1   7  // GPA7: Spare
  // GPB0-GPB7 (pins 8-15) available for future expansion
  
  // MCP23017 instance
  Adafruit_MCP23X17 mcp;
#endif

//==============================================================================
// BTS7960 Motor Driver Class - OOP Approach
//==============================================================================

/**
 * @class BTS7960Motor
 * @brief Object-oriented control for BTS7960 H-Bridge motor driver
 * 
 * This class encapsulates all motor control logic for a single BTS7960 driver:
 * - Bidirectional PWM control (RPWM for forward, LPWM for reverse)
 * - Speed ramping for smooth acceleration/deceleration
 * - Current monitoring via R_IS and L_IS pins
 * - Individual motor enable/disable
 * 
 * Inspired by: https://github.com/dan-gearscodeandfire/BTS79600_driver_code
 * Enhanced with: Safety features, current sensing, ramping control
 */
class BTS7960Motor {
private:
  // Pin assignments
  int _rpwm_pin;           ///< GPIO pin for forward PWM (RPWM)
  int _lpwm_pin;           ///< GPIO pin for reverse PWM (LPWM)
  int _r_is_pin;           ///< GPIO pin for forward current sense (R_IS) - optional
  int _l_is_pin;           ///< GPIO pin for reverse current sense (L_IS) - optional
  
  // Motor state
  int _currentSpeed;       ///< Current PWM value (-255 to +255)
  int _targetSpeed;        ///< Target PWM value for ramping
  bool _enabled;           ///< Motor enabled flag
  String _name;            ///< Motor name for debugging (e.g., "FL", "FR")
  
  // Telemetry
  float _forwardCurrent;   ///< Current when driving forward (from R_IS)
  float _reverseCurrent;   ///< Current when driving reverse (from L_IS)
  float _totalCurrent;     ///< Combined current
  
  // Configuration
  int _pwmFreq;            ///< PWM frequency in Hz
  int _pwmRes;             ///< PWM resolution in bits
  int _rampStep;           ///< PWM increment per ramp cycle
  
  /**
   * @brief Write PWM values to hardware pins
   * @param speed Speed value (-255 to +255)
   */
  void writePWM(int speed) {
    if (!_enabled) {
      // Motor disabled - set both PWM to 0
      ledcWrite(_rpwm_pin, 0);
      ledcWrite(_lpwm_pin, 0);
      return;
    }
    
    if (speed > 0) {
      // Forward direction
      ledcWrite(_rpwm_pin, abs(speed));
      ledcWrite(_lpwm_pin, 0);
    } else if (speed < 0) {
      // Reverse direction
      ledcWrite(_rpwm_pin, 0);
      ledcWrite(_lpwm_pin, abs(speed));
    } else {
      // Stop (brake)
      ledcWrite(_rpwm_pin, 0);
      ledcWrite(_lpwm_pin, 0);
    }
  }

public:
  /**
   * @brief Constructor for BTS7960Motor
   * @param name Motor name for debugging (e.g., "FL", "FR", "RL", "RR")
   * @param rpwm_pin GPIO pin for forward PWM
   * @param lpwm_pin GPIO pin for reverse PWM
   * @param r_is_pin GPIO pin for forward current sense (optional, -1 to disable)
   * @param l_is_pin GPIO pin for reverse current sense (optional, -1 to disable)
   */
  BTS7960Motor(String name, int rpwm_pin, int lpwm_pin, int r_is_pin = -1, int l_is_pin = -1) {
    _name = name;
    _rpwm_pin = rpwm_pin;
    _lpwm_pin = lpwm_pin;
    _r_is_pin = r_is_pin;
    _l_is_pin = l_is_pin;
    
    _currentSpeed = 0;
    _targetSpeed = 0;
    _enabled = true;
    
    _forwardCurrent = 0.0;
    _reverseCurrent = 0.0;
    _totalCurrent = 0.0;
    
    // Default configuration
    _pwmFreq = 1000;  // 1 kHz
    _pwmRes = 8;      // 8-bit (0-255)
    _rampStep = 10;   // Ramp increment
  }
  
  /**
   * @brief Initialize motor driver hardware
   * @param pwmFreq PWM frequency in Hz (default: 1000)
   * @param pwmRes PWM resolution in bits (default: 8)
   * @param rampStep Ramping speed increment (default: 10)
   * @return true if initialization successful, false otherwise
   */
  bool begin(int pwmFreq = 1000, int pwmRes = 8, int rampStep = 10) {
    _pwmFreq = pwmFreq;
    _pwmRes = pwmRes;
    _rampStep = rampStep;
    
    // Initialize RPWM (forward direction)
    bool rpwm_success = ledcAttach(_rpwm_pin, _pwmFreq, _pwmRes);
    if (!rpwm_success) {
      Serial.println("ERROR: Failed to attach RPWM to " + _name + " (GPIO " + String(_rpwm_pin) + ")");
      return false;
    }
    ledcWrite(_rpwm_pin, 0); // Initialize to 0 speed
    
    // Initialize LPWM (reverse direction)
    bool lpwm_success = ledcAttach(_lpwm_pin, _pwmFreq, _pwmRes);
    if (!lpwm_success) {
      Serial.println("ERROR: Failed to attach LPWM to " + _name + " (GPIO " + String(_lpwm_pin) + ")");
      return false;
    }
    ledcWrite(_lpwm_pin, 0); // Initialize to 0 speed
    
    // Initialize current sense pins (if provided)
    if (_r_is_pin >= 0) {
      pinMode(_r_is_pin, INPUT);
    }
    if (_l_is_pin >= 0) {
      pinMode(_l_is_pin, INPUT);
    }
    
    Serial.println(_name + " motor initialized: RPWM=" + String(_rpwm_pin) + 
                   " LPWM=" + String(_lpwm_pin) + " @ " + String(_pwmFreq) + "Hz");
    return true;
  }
  
  /**
   * @brief Set target motor speed (with ramping)
   * @param speed Target speed (-255 to +255, negative = reverse)
   */
  void setTargetSpeed(int speed) {
    // Constrain to valid range
    speed = constrain(speed, -255, 255);
    
    // If motor disabled, force target to 0
    if (!_enabled) {
      speed = 0;
    }
    
    _targetSpeed = speed;
  }
  
  /**
   * @brief Update motor speed with ramping (call periodically)
   * 
   * This function gradually moves currentSpeed toward targetSpeed to provide
   * smooth acceleration and deceleration, preventing gear damage.
   * Should be called from main loop at regular intervals.
   */
  void updateRamping() {
    int speedDiff = _targetSpeed - _currentSpeed;
    
    if (speedDiff == 0) {
      return; // Already at target
    }
    
    // Determine ramp direction and step size
    int rampStep = _rampStep;
    if (abs(speedDiff) < _rampStep) {
      // Close to target - use smaller step to avoid overshooting
      rampStep = abs(speedDiff);
    }
    
    // Apply ramping
    if (speedDiff > 0) {
      _currentSpeed += rampStep;
    } else {
      _currentSpeed -= rampStep;
    }
    
    // Constrain to valid range
    _currentSpeed = constrain(_currentSpeed, -255, 255);
    
    // Write to hardware
    writePWM(_currentSpeed);
  }
  
  /**
   * @brief Immediately stop motor (no ramping)
   */
  void emergencyStop() {
    _currentSpeed = 0;
    _targetSpeed = 0;
    writePWM(0);
  }
  
  /**
   * @brief Enable motor
   */
  void enable() {
    _enabled = true;
  }
  
  /**
   * @brief Disable motor
   */
  void disable() {
    _enabled = false;
    emergencyStop();
  }
  
  /**
   * @brief Check if motor is enabled
   * @return true if enabled, false otherwise
   */
  bool isEnabled() {
    return _enabled;
  }
  
  /**
   * @brief Read current from BTS7960 current sense pins
   * @param sensitivity Current sensitivity in V/A (typically 0.001 = 1mV/A)
   * @param vref ADC reference voltage (typically 3.3V)
   * @param adcMax Maximum ADC value (4095 for 12-bit)
   */
  void readCurrent(float sensitivity = 0.001, float vref = 3.3, int adcMax = 4095) {
    // Read forward current (R_IS)
    if (_r_is_pin >= 0) {
      int adc_value = analogRead(_r_is_pin);
      float voltage = (adc_value / (float)adcMax) * vref;
      _forwardCurrent = voltage / sensitivity;
    } else {
      _forwardCurrent = 0.0;
    }
    
    // Read reverse current (L_IS)
    if (_l_is_pin >= 0) {
      int adc_value = analogRead(_l_is_pin);
      float voltage = (adc_value / (float)adcMax) * vref;
      _reverseCurrent = voltage / sensitivity;
    } else {
      _reverseCurrent = 0.0;
    }
    
    // Calculate total current
    _totalCurrent = _forwardCurrent + _reverseCurrent;
  }
  
  // Getters for telemetry
  int getCurrentSpeed() { return _currentSpeed; }
  int getTargetSpeed() { return _targetSpeed; }
  float getForwardCurrent() { return _forwardCurrent; }
  float getReverseCurrent() { return _reverseCurrent; }
  float getTotalCurrent() { return _totalCurrent; }
  String getName() { return _name; }
  bool getDirection() { return (_currentSpeed >= 0); } // true = forward, false = reverse
};

//==============================================================================
// Pin Definitions
//==============================================================================

// Motor driver GPIO pins
#define FL_RPWM       25
#define FL_LPWM       26
#define FR_RPWM       27
#define FR_LPWM       14
#define RL_RPWM       12
#define RL_LPWM       13
#define RR_RPWM       32
#define RR_LPWM       33

// Current sense GPIO pins (optional)
#define FL_R_IS       39  // Note: shared with SHIFTER_D in manual mode
#define FL_L_IS       36
#define FR_R_IS       34  // Note: shared with PEDAL in manual mode
// FR_L_IS: GPIO 35 used for battery voltage
// RL/RR current sense: GPIO conflicts, not used

// Global motor enable
#define MOTOR_ENABLE  21

// Telemetry
#define VOLTAGE_PIN   35
#define TEMP_FAULT_PIN 19

// Manual control inputs
#define PEDAL_PIN      34  // Shared with FR_R_IS
#define SHIFTER_D_PIN  39  // Shared with FL_R_IS
#define SHIFTER_R_PIN   5

// ELRS receiver (Serial2)
// RX: GPIO 16, TX: GPIO 17 (configured in setup)

// Lights and audio
#define LIGHT_PIN      22
#define AUDIO_PLAY_PIN   23  // Repurposed for I2C SDA if MCP23017 used
#define AUDIO_VOLUP_PIN  18  // Repurposed for I2C SCL if MCP23017 used
#define AUDIO_VOLDOWN_PIN 4

// CRSF channels
#define THROTTLE_CH      0
#define STEERING_CH      1
#define HORN_CH          2
#define MODE_SWITCH_CH   3
#define LIGHTS_CH        5
#define SPEEDMODE_CH     6

//==============================================================================
// Constants & Configuration
//==============================================================================

// Motor control
const int PWM_FREQ         = 1000;
const int PWM_RES          = 8;
const int MAX_PWM          = 255;
const int RAMP_STEP        = 10;
const int RAMP_DELAY_MS    = 20;

// CRSF thresholds
const int MODE_THRESHOLD   = 1500;
const int LIGHT_THRESHOLD  = 1500;
const int SPEEDMODE_LOW    = 1300;
const int SPEEDMODE_HIGH   = 1700;

// Safety thresholds
const float VOLTAGE_DIVIDER_RATIO = 9.333;
const float LOW_VOLTAGE_CUTOFF    = 19.0;
const float LOW_VOLTAGE_WARNING   = 21.0;
const float ADC_VREF              = 3.3;
const int   ADC_MAX_VALUE         = 4095;
const float MAX_CURRENT_LIMIT     = 40.0;
const float HIGH_CURRENT_WARNING  = 35.0;
const unsigned long RADIO_TIMEOUT_MS = 1000;
const unsigned long TEMP_RECOVERY_MS = 30000;

// BTS7960 current sense
const float BTS7960_CURRENT_SENSITIVITY = 0.001; // 1mV/A

// Development mode
#define DEVELOPMENT_MODE false

//==============================================================================
// Enums
//==============================================================================

enum SpeedMode { REVERSE, FORWARD_LOW, FORWARD_FAST };
enum ShifterMode { DRIVE, PARK, REVERSE_MODE };
enum ControlMode { MANUAL, REMOTE };

//==============================================================================
// Global Variables
//==============================================================================

// Motor instances (OOP approach!)
BTS7960Motor motorFL("FL", FL_RPWM, FL_LPWM, FL_R_IS, FL_L_IS);
BTS7960Motor motorFR("FR", FR_RPWM, FR_LPWM, FR_R_IS, -1);  // No L_IS (GPIO 35 used)
BTS7960Motor motorRL("RL", RL_RPWM, RL_LPWM);  // No current sense (GPIO conflicts)
BTS7960Motor motorRR("RR", RR_RPWM, RR_LPWM);  // No current sense (GPIO conflicts)

// System state
SpeedMode speedMode = FORWARD_LOW;
ShifterMode shifterState = PARK;
ControlMode controlMode = MANUAL;

// CRSF input channels
int throttleInput   = 0;
int steeringInput   = 0;
int hornInput       = 0;
int modeInput       = 0;
int lightsInput     = 0;
int speedModeInput  = 0;

// System flags
bool overTemp       = false;
bool lightsOn       = false;
bool motorsEnabled  = true;

// Telemetry
float batteryVoltage = 0.0;
float motorCurrent   = 0.0;
unsigned long lastRadioSignal = 0;
unsigned long tempFaultTime    = 0;

// CRSF receiver instance
AlfredoCRSF crsf;

//==============================================================================
// Function Declarations
//==============================================================================

void readTelemetry();
void checkSafetyConditions();
void outputTelemetry();
void remoteControlLogic();
void manualControlLogic();
void readShifterState();
void setAllMotorsSpeed(int targetPwm, bool forward);
void updateAllMotorsRamping();
void emergencyStopAll();
void logMessage(const String &msg);

//==============================================================================
// SETUP
//==============================================================================

void setup() {
  // Serial communication
  Serial.begin(115200);
  delay(100);
  logMessage("========================================");
  logMessage("ESP32 Power Wheels Controller v2.2 (OOP)");
  logMessage("========================================");
  
  // UART2 for CRSF receiver
  Serial2.begin(420000, SERIAL_8N1, 16, 17);
  crsf.begin(Serial2);
  lastRadioSignal = millis();
  logMessage("CRSF receiver initialized on Serial2");

  // ADC configuration
  analogReadResolution(12);
  
  // Motor enable pin
  pinMode(MOTOR_ENABLE, OUTPUT);
  digitalWrite(MOTOR_ENABLE, HIGH);
  logMessage("BTS7960 global enable: HIGH (GPIO 21)");
  
  // Initialize all 4 motors (OOP approach!)
  logMessage("Initializing motors...");
  bool allMotorsOK = true;
  allMotorsOK &= motorFL.begin(PWM_FREQ, PWM_RES, RAMP_STEP);
  allMotorsOK &= motorFR.begin(PWM_FREQ, PWM_RES, RAMP_STEP);
  allMotorsOK &= motorRL.begin(PWM_FREQ, PWM_RES, RAMP_STEP);
  allMotorsOK &= motorRR.begin(PWM_FREQ, PWM_RES, RAMP_STEP);
  
  if (!allMotorsOK) {
    logMessage("ERROR: Motor initialization failed!");
    motorsEnabled = false;
  } else {
    logMessage("All motors initialized successfully");
  }
  
  // Telemetry inputs
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(PEDAL_PIN, INPUT);
  pinMode(TEMP_FAULT_PIN, INPUT_PULLUP);
  pinMode(SHIFTER_D_PIN, INPUT_PULLUP);
  pinMode(SHIFTER_R_PIN, INPUT_PULLUP);
  logMessage("Telemetry and control inputs configured");
  
  // Lights and audio
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);
  pinMode(AUDIO_PLAY_PIN, OUTPUT);
  pinMode(AUDIO_VOLUP_PIN, OUTPUT);
  pinMode(AUDIO_VOLDOWN_PIN, OUTPUT);
  logMessage("Lights and audio pins configured");
  
  #ifdef USE_MCP23017_EXPANDER
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    if (mcp.begin_I2C()) {
      logMessage("MCP23017 I/O expander initialized");
    } else {
      logMessage("WARNING: MCP23017 not found");
    }
  #endif
  
  logMessage("========================================");
  logMessage("Setup complete. System ready.");
  logMessage("========================================");
  delay(500);
}

//==============================================================================
// MAIN LOOP
//==============================================================================

void loop() {
  static unsigned long lastTelemetry = 0;
  static unsigned long lastRampUpdate = 0;

  // Update CRSF receiver
  crsf.update();
  
  // Read telemetry
  readTelemetry();
  
  // Read CRSF channels
  throttleInput  = crsf.getChannel(THROTTLE_CH);
  steeringInput  = crsf.getChannel(STEERING_CH);
  hornInput      = crsf.getChannel(HORN_CH);
  modeInput      = crsf.getChannel(MODE_SWITCH_CH);
  lightsInput    = crsf.getChannel(LIGHTS_CH);
  speedModeInput = crsf.getChannel(SPEEDMODE_CH);
  
  // Update radio signal timestamp
  if (throttleInput >= 900 && throttleInput <= 2100) {
    lastRadioSignal = millis();
  }

  // Determine control mode
  controlMode = (modeInput > MODE_THRESHOLD) ? REMOTE : MANUAL;
  lightsOn    = (lightsInput > LIGHT_THRESHOLD);
  readShifterState();

  // Safety checks
  checkSafetyConditions();
  
  if (!motorsEnabled) {
    emergencyStopAll();
  }

  // Execute control logic
  if (motorsEnabled) {
    if (controlMode == REMOTE) {
      remoteControlLogic();
    } else {
      manualControlLogic();
    }
  }

  // Motor speed ramping (OOP approach!)
  if (millis() - lastRampUpdate >= RAMP_DELAY_MS) {
    updateAllMotorsRamping();
    lastRampUpdate = millis();
  }

  // Periodic telemetry output
  if (millis() - lastTelemetry >= 1000) {
    outputTelemetry();
    lastTelemetry = millis();
  }

  delay(10); // ~100Hz loop
}

//==============================================================================
// FUNCTIONS
//==============================================================================

void readTelemetry() {
  if (DEVELOPMENT_MODE) {
    batteryVoltage = 24.0;
    motorCurrent = 10.0;
    overTemp = false;
    return;
  }
  
  // Battery voltage
  int voltageADC = analogRead(VOLTAGE_PIN);
  float voltageAtPin = (voltageADC / (float)ADC_MAX_VALUE) * ADC_VREF;
  batteryVoltage = voltageAtPin * VOLTAGE_DIVIDER_RATIO;
  
  if (batteryVoltage < 15.0 || batteryVoltage > 30.0) {
    batteryVoltage = 0.0; // Sensor error
  }
  
  // Motor current (OOP approach - read from each motor)
  if (controlMode == REMOTE) {
    // Only read current sense in REMOTE mode (GPIO conflicts in MANUAL)
    motorFL.readCurrent(BTS7960_CURRENT_SENSITIVITY, ADC_VREF, ADC_MAX_VALUE);
    motorFR.readCurrent(BTS7960_CURRENT_SENSITIVITY, ADC_VREF, ADC_MAX_VALUE);
  }
  
  // Calculate total current
  motorCurrent = motorFL.getTotalCurrent() + motorFR.getTotalCurrent();
  if (controlMode == REMOTE) {
    motorCurrent *= 2.0; // Estimate for all 4 motors
  }
  
  if (motorCurrent < 0.0) motorCurrent = 0.0;
  if (motorCurrent > 100.0) {
    logMessage("WARNING: Current reading unreasonable (" + String(motorCurrent, 1) + "A)");
  }
  
  // Temperature fault
  overTemp = (digitalRead(TEMP_FAULT_PIN) == LOW);
  
  if (overTemp && tempFaultTime == 0) {
    tempFaultTime = millis();
    logMessage("!!! OVER-TEMPERATURE FAULT DETECTED !!!");
  }
  
  if (!overTemp && tempFaultTime > 0) {
    if (millis() - tempFaultTime >= TEMP_RECOVERY_MS) {
      logMessage("Temperature fault cleared. System recovered.");
      tempFaultTime = 0;
    } else {
      overTemp = true; // Keep fault active during recovery
    }
  }
}

void checkSafetyConditions() {
  bool safetyOK = true;
  String warningMsg = "";
  
  // Low voltage
  if (batteryVoltage > 0 && batteryVoltage < LOW_VOLTAGE_CUTOFF) {
    safetyOK = false;
    warningMsg = "SAFETY: Low battery voltage (" + String(batteryVoltage, 2) + "V)";
  }
  
  // Overcurrent
  if (motorCurrent > MAX_CURRENT_LIMIT) {
    safetyOK = false;
    warningMsg = "SAFETY: Overcurrent detected (" + String(motorCurrent, 2) + "A)";
  }
  
  // Thermal
  if (overTemp) {
    safetyOK = false;
    static unsigned long lastWarning = 0;
    if (millis() - lastWarning >= 3000) {
      warningMsg = "SAFETY: Over-temperature fault";
      lastWarning = millis();
    }
  }
  
  // Radio failsafe
  if (controlMode == REMOTE) {
    if (millis() - lastRadioSignal > RADIO_TIMEOUT_MS) {
      safetyOK = false;
      static unsigned long lastWarning = 0;
      if (millis() - lastWarning >= 1000) {
        warningMsg = "SAFETY: Radio signal lost";
        lastWarning = millis();
      }
    }
  }
  
  // Update motor enable state
  if (!safetyOK && motorsEnabled) {
    motorsEnabled = false;
    emergencyStopAll();
    // Disable all motor objects
    motorFL.disable();
    motorFR.disable();
    motorRL.disable();
    motorRR.disable();
    if (warningMsg.length() > 0) {
      logMessage(warningMsg);
    }
  } else if (safetyOK && !motorsEnabled) {
    motorsEnabled = true;
    // Re-enable all motor objects
    motorFL.enable();
    motorFR.enable();
    motorRL.enable();
    motorRR.enable();
    logMessage("SAFETY: All conditions OK - Motors re-enabled");
  }
}

void outputTelemetry() {
  String status = "Status: ";
  status += (controlMode == REMOTE) ? "REMOTE" : "MANUAL";
  status += " | Lights: " + String(lightsOn ? "ON" : "OFF");
  status += " | Shifter: ";
  status += (shifterState == DRIVE) ? "DRIVE" : 
            (shifterState == REVERSE_MODE) ? "REVERSE" : "PARK";
  status += " | Direction: " + String(motorFL.getDirection() ? "FWD" : "REV");
  logMessage(status);
  
  String telemetry = "Power: ";
  telemetry += "Battery=" + String(batteryVoltage, 2) + "V";
  telemetry += " | Total I=" + String(motorCurrent, 2) + "A";
  telemetry += " | Motors=" + String(motorsEnabled ? "ON" : "OFF");
  telemetry += " | PWM=" + String(motorFL.getCurrentSpeed()) + "/" + String(motorFL.getTargetSpeed());
  logMessage(telemetry);
  
  // Per-motor telemetry (OOP approach!)
  String motorCurrents = "Motor I: ";
  motorCurrents += "FL=" + String(motorFL.getTotalCurrent(), 2) + "A";
  motorCurrents += " | FR=" + String(motorFR.getTotalCurrent(), 2) + "A";
  motorCurrents += " | RL=" + String(motorRL.getTotalCurrent(), 2) + "A";
  motorCurrents += " | RR=" + String(motorRR.getTotalCurrent(), 2) + "A";
  logMessage(motorCurrents);
  
  if (overTemp) {
    logMessage("WARNING: Over-temperature fault active!");
  }
}

void remoteControlLogic() {
  digitalWrite(LIGHT_PIN, lightsOn ? HIGH : LOW);
  
  // Speed mode
  if (speedModeInput < SPEEDMODE_LOW) {
    speedMode = REVERSE;
  } else if (speedModeInput < SPEEDMODE_HIGH) {
    speedMode = FORWARD_LOW;
  } else {
    speedMode = FORWARD_FAST;
  }
  
  bool forward = (speedMode != REVERSE);
  
  // Throttle mapping
  int pwm = map(throttleInput, 1000, 2000, 0, MAX_PWM);
  pwm = constrain(pwm, 0, MAX_PWM);
  
  // Speed limiting for FORWARD_LOW
  if (speedMode == FORWARD_LOW) {
    pwm = pwm / 2;
  }
  
  // Set all motors (OOP approach!)
  setAllMotorsSpeed(pwm, forward);
}

void manualControlLogic() {
  digitalWrite(LIGHT_PIN, HIGH);
  
  int pedalADC = analogRead(PEDAL_PIN);
  int pwm = map(pedalADC, 0, 4095, 0, MAX_PWM);
  pwm = constrain(pwm, 0, MAX_PWM);
  
  if (shifterState == PARK) {
    setAllMotorsSpeed(0, true);
    return;
  }
  
  bool forward = (shifterState == DRIVE);
  setAllMotorsSpeed(pwm, forward);
}

void readShifterState() {
  bool dPressed = (digitalRead(SHIFTER_D_PIN) == LOW);
  bool rPressed = (digitalRead(SHIFTER_R_PIN) == LOW);
  
  if (dPressed && !rPressed) {
    shifterState = DRIVE;
  } else if (rPressed && !dPressed) {
    shifterState = REVERSE_MODE;
  } else {
    shifterState = PARK;
  }
}

void setAllMotorsSpeed(int targetPwm, bool forward) {
  if (!motorsEnabled) {
    targetPwm = 0;
  }
  
  targetPwm = constrain(targetPwm, 0, MAX_PWM);
  
  // Convert to signed speed (-255 to +255)
  int signedSpeed = forward ? targetPwm : -targetPwm;
  
  // Set target for all motors (OOP approach!)
  motorFL.setTargetSpeed(signedSpeed);
  motorFR.setTargetSpeed(signedSpeed);
  motorRL.setTargetSpeed(signedSpeed);
  motorRR.setTargetSpeed(signedSpeed);
}

void updateAllMotorsRamping() {
  // Update ramping for all motors (OOP approach!)
  motorFL.updateRamping();
  motorFR.updateRamping();
  motorRL.updateRamping();
  motorRR.updateRamping();
}

void emergencyStopAll() {
  // Emergency stop all motors (OOP approach!)
  motorFL.emergencyStop();
  motorFR.emergencyStop();
  motorRL.emergencyStop();
  motorRR.emergencyStop();
}

void logMessage(const String &msg) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(msg);
}

//==============================================================================
// END OF CODE
//==============================================================================

