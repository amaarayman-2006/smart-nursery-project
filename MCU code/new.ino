#include <Servo.h>

#define PIN_TEMP_AO      PA0    // NTC Thermistor (Analog)
#define PIN_LDR_DO       PA1    // LDR Sensor (Digital)
#define PIN_GAS_AO       PA2    // MQ-5 Gas Sensor (Analog)
#define PIN_PIR_OUT      PA3    // HC-SR501 Motion (Digital)

#define PIN_ROOM_LED     PA4    // Room Illumination LED
#define PIN_BUZZER       PA5    // Safety & Warning Buzzer

#define PIN_L293D_EN1    PA6    // L293D Enable (PWM Fan Speed)
#define PIN_L293D_IN1    PA7    // L293D Input 1
#define PIN_L293D_IN2    PB0    // L293D Input 2

#define PIN_SERVO   PA8    // TIM1_CH1 

#define PIN_RGB_R        PB4    // RGB Module Red
#define PIN_RGB_G        PB5    // RGB Module Green
#define PIN_RGB_B        PB6    // RGB Module Blue

#define ADC_RESOLUTION     4095.0f
#define GAS_ALARM_LIMIT    1000

#define MAX_MOTION_EVENTS  10
unsigned long motionTimestamps[MAX_MOTION_EVENTS];
int           motionIndex     = 0;
bool          previousPirState = LOW;
unsigned long lastMotionTime  = 0;
bool          isBabyAwake     = false;

bool isCryDetected          = false;
bool isServoRocking         = false;
bool isBuzzerActiveFromLaptop = false;
bool isGasAlertActive       = false;

Servo         cribServo;
int           currentServoAngle  = 90;
int           servoDirection     = 1;
unsigned long lastServoMoveTime  = 0;
const int     SERVO_SWEEP_INTERVAL = 15;

void  runDiagnosticTest();
void  readSerialCommands();
void  processMotionDetection();
void  processRoomLighting();
void  processTemperatureAndCooling();
void  processGasSafetyAlert();
void  processServoRocking();
void  updateBuzzerState();
void  setRGBColor(uint8_t red, uint8_t green, uint8_t blue);
float readTemperatureCelsius();

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  pinMode(PIN_LDR_DO,  INPUT);
  pinMode(PIN_PIR_OUT, INPUT);

  pinMode(PIN_ROOM_LED,  OUTPUT);
  pinMode(PIN_BUZZER,    OUTPUT);
  pinMode(PIN_L293D_EN1, OUTPUT);
  pinMode(PIN_L293D_IN1, OUTPUT);
  pinMode(PIN_L293D_IN2, OUTPUT);
  pinMode(PIN_RGB_R,     OUTPUT);
  pinMode(PIN_RGB_G,     OUTPUT);
  pinMode(PIN_RGB_B,     OUTPUT);

  digitalWrite(PIN_L293D_IN1, HIGH);
  digitalWrite(PIN_L293D_IN2, LOW);
  analogWrite(PIN_L293D_EN1, 0);

  cribServo.attach(PIN_SERVO);
  cribServo.write(90);

  for (int i = 0; i < MAX_MOTION_EVENTS; i++) {
    motionTimestamps[i] = 0;
  }

  runDiagnosticTest();
}


void loop() {
  readSerialCommands();           
  processGasSafetyAlert();        
  processMotionDetection();      
  processRoomLighting();          
  processTemperatureAndCooling(); 
  processServoRocking();          
  updateBuzzerState();            
}


void runDiagnosticTest() {
  Serial.println();
  Serial.println("=========================================");
  Serial.println("  SMART NURSERY GUARDIAN - SELF TEST");
  Serial.println("=========================================");
  Serial.println("Testing all connections. Observe each");
  Serial.println("component as it is triggered...");
  Serial.println("-----------------------------------------");

  int passCount = 0;
  int warnCount = 0;
  int failCount = 0;

  Serial.print("[SENSOR]  Thermistor  (PA0) ... ");
  int   tempRaw = analogRead(PIN_TEMP_AO);
  float tempC   = readTemperatureCelsius();

  if (tempRaw <= 10 || tempRaw >= 4085) {
    Serial.print("[WARN]  ADC stuck at extreme (");
    Serial.print(tempRaw);
    Serial.println("). Check thermistor wiring and 10k pull-up resistor.");
    warnCount++;
  } else if (tempC < -10.0f || tempC > 80.0f) {
    Serial.print("[FAIL]  Unrealistic temperature: ");
    Serial.print(tempC, 1);
    Serial.println(" C. Check Beta constant or wiring polarity.");
    failCount++;
  } else {
    Serial.print("[PASS]  ");
    Serial.print(tempC, 1);
    Serial.println(" C");
    passCount++;
  }
  delay(100);

  Serial.print("[SENSOR]  LDR         (PA1) ... ");
  int ldrState = digitalRead(PIN_LDR_DO);
  if (ldrState == HIGH) {
    Serial.println("[PASS]  DO = LOW  (Dark / below threshold)");
  } else {
    Serial.println("[PASS]  DO = HIGH (Bright / above threshold)");
  }
  passCount++;
  delay(100);

  Serial.print("[SENSOR]  Gas Sensor  (PA2) ... ");
  int gasRaw = analogRead(PIN_GAS_AO);

  if (gasRaw <= 10) {
    Serial.print("[WARN]  ADC near zero (");
    Serial.print(gasRaw);
    Serial.println("). Sensor may be unpowered or pin disconnected.");
    warnCount++;
  } else if (gasRaw >= GAS_ALARM_LIMIT) {
    Serial.print("[WARN]  Above alarm threshold (");
    Serial.print(gasRaw);
    Serial.println("). Ventilate area before normal operation.");
    warnCount++;
  } else {
    Serial.print("[PASS]  ADC = ");
    Serial.print(gasRaw);
    Serial.println(" (safe range)");
    passCount++;
  }
  delay(100);

  Serial.print("[SENSOR]  PIR Sensor  (PA3) ... ");
  int pirState = digitalRead(PIN_PIR_OUT);
  Serial.print("[PASS]  OUT = ");
  Serial.println(pirState == HIGH
    ? "HIGH (Motion detected)"
    : "LOW  (No motion — expected at boot)");
  passCount++;
  delay(100);

  Serial.println("-----------------------------------------");

  Serial.print("[ACTUATOR] Room LED    (PA4) ... ");
  digitalWrite(PIN_ROOM_LED, HIGH);
  delay(500);
  digitalWrite(PIN_ROOM_LED, LOW);
  Serial.println("[CHECK] LED should have lit for 0.5 s.");

  Serial.print("[ACTUATOR] Buzzer      (PA5) ... ");
  digitalWrite(PIN_BUZZER, HIGH);
  delay(200);
  digitalWrite(PIN_BUZZER, LOW);
  Serial.println("[CHECK] You should have heard a 0.2 s beep.");

  Serial.print("[ACTUATOR] Fan/L293D   (PA6) ... ");
  digitalWrite(PIN_L293D_IN1, HIGH);
  digitalWrite(PIN_L293D_IN2, LOW);
  analogWrite(PIN_L293D_EN1, 180); 
  delay(600);
  analogWrite(PIN_L293D_EN1, 0);   
  Serial.println("[CHECK] Fan should have spun for 0.6 s.");

  Serial.print("[ACTUATOR] Servo       (PB1) ... ");
  cribServo.write(135); delay(500); 
  cribServo.write(45);  delay(500); 
  cribServo.write(90);  delay(300); 
  Serial.println("[CHECK] Servo should have swept right > left > center.");

  Serial.print("[ACTUATOR] RGB LED  (PB4-6) ... ");
  setRGBColor(255, 0,   0);   delay(400); 
  setRGBColor(0,   255, 0);   delay(400); 
  setRGBColor(0,   0,   255); delay(400); 
  setRGBColor(0,   0,   0);               
  Serial.println("[CHECK] RGB should have flashed Red > Green > Blue.");

  Serial.println("-----------------------------------------");
  Serial.println("  DIAGNOSTIC SUMMARY");
  Serial.println("-----------------------------------------");
  Serial.print("  Sensors:   ");
  Serial.print(passCount); Serial.print(" PASS | ");
  Serial.print(warnCount); Serial.print(" WARN | ");
  Serial.print(failCount); Serial.println(" FAIL");
  Serial.println("  Actuators: Verify visually/audibly above.");
  Serial.println();

  if (failCount > 0) {
    Serial.println("  !! FAIL detected. Fix wiring before use.");
  } else if (warnCount > 0) {
    Serial.println("  ** WARNings present. Review flagged sensors.");
  } else {
    Serial.println("  All sensor checks PASSED.");
  }

  Serial.println("=========================================");
  Serial.println("  Starting main loop in 3 seconds...");
  Serial.println("=========================================");
  delay(3000);
}


void readSerialCommands() {
  while (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "CRY_DETECTED") {
      isCryDetected  = true;
      isServoRocking = true;
    }
    else if (command == "CRY_ENDED") {
      isCryDetected  = false;
      isServoRocking = false;
    }
    else if (command == "SERVO_START") {
      isServoRocking = true;
    }
    else if (command == "SERVO_STOP") {
      isServoRocking = false;
    }
    else if (command == "BUZZER_ON") {
      isBuzzerActiveFromLaptop = true;
    }
    else if (command == "BUZZER_OFF") {
      isBuzzerActiveFromLaptop = false;
    }
  }
}


void processMotionDetection() {
  bool          currentPirState = digitalRead(PIN_PIR_OUT);
  unsigned long now             = millis();

  if (currentPirState == HIGH && previousPirState == LOW && (now - lastMotionTime > 200)) {
    motionTimestamps[motionIndex] = now;
    motionIndex = (motionIndex + 1) % MAX_MOTION_EVENTS;
    lastMotionTime  = now;
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

  bool newlyAwake = (validMotionCount >= 4);

  if (newlyAwake && !isBabyAwake) {
    isBabyAwake = true;
    Serial.println("BABY_AWAKE");
  } else if (!newlyAwake && isBabyAwake) {
    isBabyAwake = false;
    Serial.println("BABY_ASLEEP");
  }
}


void processRoomLighting() {
  bool isDark = (digitalRead(PIN_LDR_DO) == LOW); 

  if (isDark && (isBabyAwake || isCryDetected)) {
    digitalWrite(PIN_ROOM_LED, HIGH);
  } else {
    digitalWrite(PIN_ROOM_LED, LOW);
  }
}


void processTemperatureAndCooling() {
  static unsigned long lastThermalReportTime = 0;
  float tempC = readTemperatureCelsius();

  if (tempC < 25.0f) {
    setRGBColor(0, 0, 255);
    analogWrite(PIN_L293D_EN1, 0);   
  } else if (tempC <= 30.0f) {
    setRGBColor(0, 255, 0);
    analogWrite(PIN_L293D_EN1, 128); 
  } else {
    setRGBColor(255, 0, 0);
    analogWrite(PIN_L293D_EN1, 255); 
  }

  if (millis() - lastThermalReportTime > 2000) {
    lastThermalReportTime = millis();
    Serial.print("TEMP:");
    Serial.println(tempC, 1);
  }
}


void processGasSafetyAlert() {
  static unsigned long lastGasAlertTime = 0;
  int gasAdcValue = analogRead(PIN_GAS_AO);

  if (gasAdcValue > GAS_ALARM_LIMIT) {
    isGasAlertActive = true;
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
    cribServo.write(90); 
    return;
  }

  unsigned long now = millis();
  if (now - lastServoMoveTime >= SERVO_SWEEP_INTERVAL) {
    lastServoMoveTime    = now;
    currentServoAngle   += servoDirection * 2;

    if (currentServoAngle >= 180) {
      currentServoAngle = 180;
      servoDirection    = -1;
    } else if (currentServoAngle <= 0) {
      currentServoAngle = 0;
      servoDirection    = 1;
    }
    cribServo.write(currentServoAngle);
  }
}


void updateBuzzerState() {
  if (isGasAlertActive) {
    digitalWrite(PIN_BUZZER, HIGH); 
  } else if (isBuzzerActiveFromLaptop) {
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

  float vOut  = rawAdc * (3.3f / ADC_RESOLUTION);
  float rNTC  = (10000.0f * vOut) / (3.3f - vOut);

  const float Beta = 3950.0f;
  const float T0   = 298.15f;
  const float R0   = 10000.0f;

  float steinhart = log(rNTC / R0);
  steinhart /= Beta;
  steinhart += 1.0f / T0;
  steinhart  = 1.0f / steinhart;
  steinhart -= 273.15f;

  return steinhart;
}
