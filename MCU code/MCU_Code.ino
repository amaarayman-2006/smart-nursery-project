#include <Servo.h>

// --- Pin Definitions for STM32 Black Pill (Timer-Safe) ---
#define PIN_TEMP_AO      PA_0   // NTC Thermistor Module Analog Out (ADC1)
#define PIN_LDR_AO       PA_1   // LDR Sensor Module Analog Out (ADC1)
#define PIN_GAS_AO       PA_2   // MQ-5 Gas Sensor Module Analog Out (ADC1)
#define PIN_PIR_OUT      PA_10  // HC-SR501 Motion Output (Moved to free up PA_3)

#define PIN_ROOM_LED     PA_4   // Room Illumination LED
#define PIN_BUZZER       PA_5   // Safety & Warning Buzzer

#define PIN_L293D_EN1    PA_6   // L293D Enable Pin (TIM3_CH1) - Fan Speed PWM
#define PIN_L293D_IN1    PA_7   // L293D Input 1
#define PIN_L293D_IN2    PB_0   // L293D Input 2

#define PIN_SERVO        PA_3   // SG90 Micro Servo (TIM2_CH4 - Isolated from TIM3 fan)

#define PIN_RGB_R        PB_3   // HW-479 RGB Module Red Pin
#define PIN_RGB_G        PB_7   // HW-479 RGB Module Green Pin (TIM4_CH2)
#define PIN_RGB_B        PB_6   // HW-479 RGB Module Blue Pin (TIM4_CH1)

// --- Operational Calibration Thresholds ---
#define ADC_RESOLUTION     4095.0f  // 12-bit ADC Resolution on STM32
#define LDR_DARK_THRESHOLD 2500     // ADC value above which room is dark
#define GAS_ALARM_LIMIT    1800     // ADC threshold for MQ-5 Gas/Smoke detection

// --- PIR Motion Sliding Window Variables ---
#define MAX_MOTION_EVENTS 10
unsigned long motionTimestamps[MAX_MOTION_EVENTS];
int motionIndex = 0;
bool previousPirState = LOW;
bool isBabyAwake = false;

// --- System State Flags ---
bool isCryDetected = false;
bool isServoRocking = false;
bool isBuzzerActive = false;

// --- Non-blocking Servo Variables ---
Servo cribServo;
int currentServoAngle = 90;
int servoDirection = 1;
unsigned long lastServoMoveTime = 0;
const int SERVO_SWEEP_INTERVAL = 15; // Delay between 1-degree steps (ms)

// --- Function Prototypes ---
void readSerialCommands();
void processMotionDetection();
void processRoomLighting();
void processTemperatureAndCooling();
void processGasSafetyAlert();
void processServoRocking();
void setRGBColor(uint8_t red, uint8_t green, uint8_t blue);
float readTemperatureCelsius();

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);

  pinMode(PIN_PIR_OUT, INPUT);
  pinMode(PIN_ROOM_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  pinMode(PIN_L293D_EN1, OUTPUT);
  pinMode(PIN_L293D_IN1, OUTPUT);
  pinMode(PIN_L293D_IN2, OUTPUT);

  pinMode(PIN_RGB_R, OUTPUT);
  pinMode(PIN_RGB_G, OUTPUT);
  pinMode(PIN_RGB_B, OUTPUT);

  // Default motor direction setup
  digitalWrite(PIN_L293D_IN1, HIGH);
  digitalWrite(PIN_L293D_IN2, LOW);
  analogWrite(PIN_L293D_EN1, 0);

  cribServo.attach(PIN_SERVO);
  cribServo.write(90);

  for (int i = 0; i < MAX_MOTION_EVENTS; i++) {
    motionTimestamps[i] = 0;
  }
}

void loop() {
  readSerialCommands();
  processMotionDetection();
  processRoomLighting();
  processTemperatureAndCooling();
  processGasSafetyAlert();
  processServoRocking();
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "CRY_DETECTED") {
      isCryDetected = true;
      isServoRocking = true;
    } else if (command == "CRY_ENDED") {
      isCryDetected = false;
      isServoRocking = false;
    } else if (command == "SERVO_START") {
      isServoRocking = true;
    } else if (command == "SERVO_STOP") {
      isServoRocking = false;
    } else if (command == "BUZZER_ON") {
      isBuzzerActive = true;
    } else if (command == "BUZZER_OFF") {
      isBuzzerActive = false;
    }
  }
}

void processMotionDetection() {
  bool currentPirState = digitalRead(PIN_PIR_OUT);
  unsigned long now = millis();

  if (currentPirState == HIGH && previousPirState == LOW) {
    motionTimestamps[motionIndex] = now;
    motionIndex = (motionIndex + 1) % MAX_MOTION_EVENTS;
    previousPirState = HIGH;
  } else if (currentPirState == LOW) {
    previousPirState = LOW;
  }

  int validMotionCount = 0;
  for (int i = 0; i < MAX_MOTION_EVENTS; i++) {
    if (motionTimestamps[i] > 0 && (now - motionTimestamps[i] <= 8000)) {
      validMotionCount++;
    }
  }

  // 4 or more motions within an 8-second window flags the baby as awake[cite: 1]
  bool newlyAwakeState = (validMotionCount >= 4);

  if (newlyAwakeState && !isBabyAwake) {
    isBabyAwake = true;
    Serial.println("BABY_AWAKE");
  } else if (!newlyAwakeState && isBabyAwake) {
    isBabyAwake = false;
    Serial.println("BABY_ASLEEP");
  }
}

void processRoomLighting() {
  int ldrValue = analogRead(PIN_LDR_AO);
  bool isDark = (ldrValue > LDR_DARK_THRESHOLD);

  // LED turns on only when dark AND (baby awake OR cry detected)[cite: 1]
  if (isDark && (isBabyAwake || isCryDetected)) {
    digitalWrite(PIN_ROOM_LED, HIGH);
  } else {
    digitalWrite(PIN_ROOM_LED, LOW);
  }
}

void processTemperatureAndCooling() {
  static unsigned long lastThermalReportTime = 0;
  float tempC = readTemperatureCelsius();

  // Temperature ranges mapped to status color and fan speed[cite: 1]
  if (tempC < 25.0f) {
    setRGBColor(0, 0, 255);       // Blue: Below 25°C (Fan Off)[cite: 1]
    analogWrite(PIN_L293D_EN1, 0);
  } else if (tempC >= 25.0f && tempC <= 30.0f) {
    setRGBColor(0, 255, 0);       // Green: 25°C to 30°C (Half speed)[cite: 1]
    analogWrite(PIN_L293D_EN1, 128);
  } else {
    setRGBColor(255, 0, 0);       // Red: Above 30°C (Full speed)[cite: 1]
    analogWrite(PIN_L293D_EN1, 255);
  }

  if (millis() - lastThermalReportTime > 2000) {
    lastThermalReportTime = millis();
    Serial.print("TEMP:");
    Serial.println(tempC, 1);
  }
}

void processGasSafetyAlert() {
  int gasAdcValue = analogRead(PIN_GAS_AO);
  static unsigned long lastGasAlertTime = 0;

  // Gas or smoke detection triggers the highest-priority alert path[cite: 1]
  if (gasAdcValue > GAS_ALARM_LIMIT) {
    digitalWrite(PIN_BUZZER, HIGH);
    if (millis() - lastGasAlertTime > 1000) {
      lastGasAlertTime = millis();
      Serial.println("GAS_ALERT");
    }
  } else {
    digitalWrite(PIN_BUZZER, isBuzzerActive ? HIGH : LOW);
  }
}

void processServoRocking() {
  if (!isServoRocking) {
    cribServo.write(90);
    return;
  }

  unsigned long now = millis();
  if (now - lastServoMoveTime >= SERVO_SWEEP_INTERVAL) {
    lastServoMoveTime = now;
    currentServoAngle += servoDirection * 2;

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
