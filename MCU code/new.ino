#include <Servo.h>

// --- SENSOR & ACTUATOR PINS ---
#define PIN_TEMP_AO      PA_0   // NTC Thermistor (Analog)
#define PIN_LDR_DO       PA_1   // LDR Sensor (Digital)
#define PIN_GAS_AO       PA_2   // MQ-5 Gas Sensor (Analog)
#define PIN_PIR_OUT      PA_3   // HC-SR501 Motion (Digital)

#define PIN_ROOM_LED     PA_4   // Room Illumination LED
#define PIN_BUZZER       PA_5   // Safety & Warning Buzzer

#define PIN_L293D_EN1    PA_6   // L293D Enable (PWM Fan Speed)
#define PIN_L293D_IN1    PA_7   // L293D Input 1
#define PIN_L293D_IN2    PB_0   // L293D Input 2

#define PIN_SERVO        PB_1   // SG90 Micro Servo Output

#define PIN_RGB_R        PB_4   // RGB Module Red
#define PIN_RGB_G        PB_5   // RGB Module Green
#define PIN_RGB_B        PB_6   // RGB Module Blue

// --- SYSTEM CONSTANTS ---
#define ADC_RESOLUTION     4095.0f  // 12-bit ADC on STM32
#define GAS_ALARM_LIMIT    1800     // Gas/Smoke threshold

// --- GLOBAL STATES ---
// PIR / Motion state
#define MAX_MOTION_EVENTS 10
unsigned long motionTimestamps[MAX_MOTION_EVENTS];
int motionIndex = 0;
bool previousPirState = LOW;
unsigned long lastMotionTime = 0;
bool isBabyAwake = false;

// System states from Laptop/Local
bool isCryDetected = false;
bool isServoRocking = false;
bool isBuzzerActiveFromLaptop = false; // For "Tired" cry
bool isGasAlertActive = false;         // Top priority alert

// Servo state
Servo cribServo;
int currentServoAngle = 90;
int servoDirection = 1;
unsigned long lastServoMoveTime = 0;
const int SERVO_SWEEP_INTERVAL = 15;

// --- FUNCTION PROTOTYPES ---
void readSerialCommands();
void processMotionDetection();
void processRoomLighting();
void processTemperatureAndCooling();
void processGasSafetyAlert();
void processServoRocking();
void updateBuzzerState();
void setRGBColor(uint8_t red, uint8_t green, uint8_t blue);
float readTemperatureCelsius();

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  // Initialize Inputs
  pinMode(PIN_LDR_DO, INPUT);
  pinMode(PIN_PIR_OUT, INPUT);

  // Initialize Outputs
  pinMode(PIN_ROOM_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  pinMode(PIN_L293D_EN1, OUTPUT);
  pinMode(PIN_L293D_IN1, OUTPUT);
  pinMode(PIN_L293D_IN2, OUTPUT);

  pinMode(PIN_RGB_R, OUTPUT);
  pinMode(PIN_RGB_G, OUTPUT);
  pinMode(PIN_RGB_B, OUTPUT);

  // Set initial Fan direction (Forward)
  digitalWrite(PIN_L293D_IN1, HIGH);
  digitalWrite(PIN_L293D_IN2, LOW);
  analogWrite(PIN_L293D_EN1, 0);

  // Attach Servo
  cribServo.attach(PIN_SERVO);
  cribServo.write(90);

  // Clear motion array
  for (int i = 0; i < MAX_MOTION_EVENTS; i++) {
    motionTimestamps[i] = 0;
  }
}

void loop() {
  readSerialCommands();           // 1. Check for Laptop instructions
  processGasSafetyAlert();        // 2. Check Gas (Highest Priority)
  processMotionDetection();       // 3. Update Baby Awake status
  processRoomLighting();          // 4. Update LED based on LDR + Awake/Cry status
  processTemperatureAndCooling(); // 5. Update Fan and RGB locally
  processServoRocking();          // 6. Move servo if active
  updateBuzzerState();            // 7. Resolve buzzer priorities
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "CRY_DETECTED") {
      isCryDetected = true;
      isServoRocking = true;      // Per .md: starts rocking immediately
    } 
    else if (command == "CRY_ENDED") {
      isCryDetected = false;
      isServoRocking = false;     // Per .md: servo stops
      isBuzzerActiveFromLaptop = false; // Clear tired cry alert
    } 
    else if (command == "SERVO_START") {
      isServoRocking = true;
    } 
    else if (command == "SERVO_STOP") {
      isServoRocking = false;
    } 
    else if (command == "BUZZER_ON") {
      isBuzzerActiveFromLaptop = true;  // Triggered by Tired Cry
    } 
    else if (command == "BUZZER_OFF") {
      isBuzzerActiveFromLaptop = false;
    }
  }
}

void processMotionDetection() {
  bool currentPirState = digitalRead(PIN_PIR_OUT);
  unsigned long now = millis();

  // Detect Rising Edge (Motion started) with a 200ms debounce
  if (currentPirState == HIGH && previousPirState == LOW && (now - lastMotionTime > 200)) {
    motionTimestamps[motionIndex] = now;
    motionIndex = (motionIndex + 1) % MAX_MOTION_EVENTS;
    lastMotionTime = now;
    previousPirState = HIGH;
  } else if (currentPirState == LOW) {
    previousPirState = LOW;
  }

  // Count valid motions in the rolling 8-second (8000ms) window
  int validMotionCount = 0;
  for (int i = 0; i < MAX_MOTION_EVENTS; i++) {
    if (motionTimestamps[i] > 0 && (now - motionTimestamps[i] <= 8000)) {
      validMotionCount++;
    }
  }

  // Evaluate 4+ motions in 8 seconds rule
  bool newlyAwakeState = (validMotionCount >= 4);

  if (newlyAwakeState && !isBabyAwake) {
    isBabyAwake = true;
    Serial.println("BABY_AWAKE"); // Report to laptop
  } else if (!newlyAwakeState && isBabyAwake) {
    isBabyAwake = false;
    Serial.println("BABY_ASLEEP"); // Report to laptop
  }
}

void processRoomLighting() {
  // Assuming the digital LDR module outputs HIGH when it is dark.
  // If your specific module turns ON when dark, change HIGH to LOW.
  bool isDark = (digitalRead(PIN_LDR_DO) == HIGH);

  // Per .md: LED ON if (Dark) AND (Awake OR Cry Detected)
  if (isDark && (isBabyAwake || isCryDetected)) {
    digitalWrite(PIN_ROOM_LED, HIGH);
  } else {
    digitalWrite(PIN_ROOM_LED, LOW);
  }
}

void processTemperatureAndCooling() {
  static unsigned long lastThermalReportTime = 0;
  float tempC = readTemperatureCelsius();

  // Per .md: Temperature dictates BOTH light color and fan speed
  if (tempC < 25.0f) {
    setRGBColor(0, 0, 255);       // Blue
    analogWrite(PIN_L293D_EN1, 0); // Fan Off
  } 
  else if (tempC >= 25.0f && tempC <= 30.0f) {
    setRGBColor(0, 255, 0);       // Green
    analogWrite(PIN_L293D_EN1, 128); // Fan Half Speed
  } 
  else {
    setRGBColor(255, 0, 0);       // Red
    analogWrite(PIN_L293D_EN1, 255); // Fan Full Speed
  }

  // Report temp to laptop every 2 seconds
  if (millis() - lastThermalReportTime > 2000) {
    lastThermalReportTime = millis();
    Serial.print("TEMP:");
    Serial.println(tempC, 1);
  }
}

void processGasSafetyAlert() {
  int gasAdcValue = analogRead(PIN_GAS_AO);
  static unsigned long lastGasAlertTime = 0;

  if (gasAdcValue > GAS_ALARM_LIMIT) {
    isGasAlertActive = true;
    
    // Spam the laptop with alerts every 1 second
    if (millis() - lastGasAlertTime > 1000) {
      lastGasAlertTime = millis();
      Serial.println("GAS_ALERT"); 
    }
  } else {
    isGasAlertActive = false;
  }
}

void processServoRocking() {
  if (!isServoRocking) {
    // If not rocking, gently return to center (90 degrees)
    cribServo.write(90);
    return;
  }

  unsigned long now = millis();
  if (now - lastServoMoveTime >= SERVO_SWEEP_INTERVAL) {
    lastServoMoveTime = now;
    currentServoAngle += servoDirection * 2; // Speed multiplier

    // Reverse direction at limits
    if (currentServoAngle >= 180) {
      currentServoAngle = 180;
      servoDirection = -1;
    } else if (currentServoAngle <= 0) {
      currentServoAngle = 0;
      servoDirection = 1;
    }
    cribServo.write(currentServoAngle);
  }
}

void updateBuzzerState() {
  // Resolve Buzzer Priorities
  if (isGasAlertActive) {
    // Highest priority overrides everything
    digitalWrite(PIN_BUZZER, HIGH);
  } else if (isBuzzerActiveFromLaptop) {
    // Second priority: Tired cry alert from laptop
    digitalWrite(PIN_BUZZER, HIGH);
  } else {
    digitalWrite(PIN_BUZZER, LOW);
  }
}

void setRGBColor(uint8_t red, uint8_t green, uint8_t blue) {
  analogWrite(PIN_RGB_R, red);
  analogWrite(PIN_RGB_G, green);
  analogWrite(PIN_RGB_B, blue);
}

float readTemperatureCelsius() {
  int rawAdc = analogRead(PIN_TEMP_AO);
  if (rawAdc == 0) return 0.0f;

  float vOut = rawAdc * (3.3f / ADC_RESOLUTION);
  float rNTC = (10000.0f * vOut) / (3.3f - vOut);

  const float Beta = 3950.0f;
  const float T0 = 298.15f;
  const float R0 = 10000.0f;

  float steinhart = rNTC / R0;
  steinhart = log(steinhart);
  steinhart /= Beta;
  steinhart += 1.0f / T0;
  steinhart = 1.0f / steinhart;
  steinhart -= 273.15f;

  return steinhart;
}
