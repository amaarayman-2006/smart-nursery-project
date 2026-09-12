# Bill of Materials (BOM)

---

## 1. Microcontroller

| Component | Specification | Qty |
|---|---|---|
| STM32F401RCT6 Dev Board | "Black Pill" — programmed via Arduino IDE (STM32duino core) | 1 |

---

## 2. Sensors

| Component | Specification | Connected Pin (per firmware) | Qty |
|---|---|---|---|
| NTC Thermistor Temperature Sensor Module | 4-pin, analog output | PA0 | 1 |
| PIR Motion Sensor Module | HC-SR501, digital output | PA3 | 1 |
| Photoresistor (LDR) Module | 4-pin, digital output (light detection) | PA1 | 1 |
| MQ-5 Gas Sensor Module | Analog output — **note:** schematic silkscreen shows "MQ-2," firmware comment says "MQ-5"; confirm which physical module is on hand | PA2 | 1 |

---

## 3. Actuators / Output Modules

| Component | Specification | Connected Pin(s) | Qty |
|---|---|---|---|
| HW-479 RGB LED Module | Common-cathode, 3-channel | R→PB4, G→PB5, B→PB6 | 1 |
| LED (room illumination) | Standard 5mm, green | PA4 (through resistor) | 1 |
| L293D Motor Driver IC | Dual H-bridge, DIP-16 | EN1→PA6, IN1→PA7, IN2→PB0 | 1 |
| F130 DC Motor | 5V | Driven via L293D | 1 |
| Red Plastic Propeller | 80mm, 3-blade | Mounted on F130 motor | 1 |
| SG90 Micro Servo | 180° rotation | PB1 | 1 |
| Passive Buzzer | 5V | PA5 (through R10 220Ω resistor) | 1 |

---

## 4. Passive Components

| Component | Value | Used For | Qty |
|---|---|---|---|
| Resistor | 220 Ω | LED (R4) + RGB channels (R5, R6, R7) | 4 |
| Resistor | 330 Ω | Buzzer (R10) | 1 |
| Resistor | 1 kΩ | Voltage divider (R8) | 1 |
| Resistor | 2 kΩ | Voltage divider (R9) — **note:** not in original supplied resistor list; confirm sourced separately | 1 |
| Resistor | 10 kΩ | LDR pull (R2), NTC pull (R3), NTC divider pair | 3+ |

---

## 5. Prototyping & Wiring

| Component | Specification | Qty |
|---|---|---|
| Breadboard | Standard full-size | 2 |
| Jumper Wires | Male-to-Male | as needed |
| Jumper Wires | Male-to-Female | as needed |

---
