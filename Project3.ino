/*
  EN.605.715.81.SP26 Software Development for Real-Time Embedded Systems
  Han Nguyen, Matthew Neufeld, Xinyu Xiao | February 20th, 2026
  Project 3 - Measure and Transmit Fan Speed and Thrust
*/

#include <Arduino.h>

// ======================
// Pin Configuration
// ======================
static const int STEPPER_PIN1 = 11;
static const int STEPPER_PIN2 = 12;
static const int STEPPER_PIN3 = 13;
static const int STEPPER_PIN4 = 14;

static const int TACH_PIN = 4;        // IR detector output → interrupt pin
static const int IR_EMITTER_PIN = 5;  // IR LED emitter (always ON)

// ======================
// Tachometer Parameters
// ======================
static const uint32_t MIN_PULSE_GAP_US = 300;           // Debounce filter (µs)
static const uint32_t STEP_DELAY_US = 2500;             // Time between motor steps (µs)
static const uint32_t RPM_CALCULATION_PERIOD_MS = 100;  // RPM sampling interval
static const int PULSES_PER_REV = 1;                    // Reflective marks per revolution

// ======================
// Half-Step Sequence
// ======================
static const uint8_t SEQ[8][4] = {
  { 1, 0, 0, 0 },
  { 1, 1, 0, 0 },
  { 0, 1, 0, 0 },
  { 0, 1, 1, 0 },
  { 0, 0, 1, 0 },
  { 0, 0, 1, 1 },
  { 0, 0, 0, 1 },
  { 1, 0, 0, 1 }
};

// ======================
// Global State
// ======================
volatile uint32_t pulseCount = 0;       // Total tach pulses detected
volatile uint32_t lastPulseTimeUs = 0;  // Timestamp of last valid pulse

int stepIndex = 0;
uint32_t lastPulseCount = 0;
uint32_t lastRpmCalculationTimeMs = 0;

// ======================
// Interrupt Service Routine
// ======================
void IRAM_ATTR tachISR() {
  uint32_t nowUs = micros();

  // Reject pulses that occur too quickly (noise filtering)
  if (nowUs - lastPulseTimeUs >= MIN_PULSE_GAP_US) {
    pulseCount++;
    lastPulseTimeUs = nowUs;
  }
}

void setup() {
  Serial.begin(115200);

  // Configure stepper pins as outputs
  pinMode(STEPPER_PIN1, OUTPUT);
  pinMode(STEPPER_PIN2, OUTPUT);
  pinMode(STEPPER_PIN3, OUTPUT);
  pinMode(STEPPER_PIN4, OUTPUT);

  // IR emitter ON continuously
  pinMode(IR_EMITTER_PIN, OUTPUT);
  digitalWrite(IR_EMITTER_PIN, HIGH);

  // Tach input pin with internal pull-up
  pinMode(TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, FALLING);
}

void loop() {

  // Stepper Motor Drive
  digitalWrite(STEPPER_PIN1, SEQ[stepIndex][0]);
  digitalWrite(STEPPER_PIN2, SEQ[stepIndex][1]);
  digitalWrite(STEPPER_PIN3, SEQ[stepIndex][2]);
  digitalWrite(STEPPER_PIN4, SEQ[stepIndex][3]);

  stepIndex = (stepIndex + 1) % 8;
  delayMicroseconds(STEP_DELAY_US);

  // RPM Calculation
  uint32_t nowMs = millis();

  if (nowMs - lastRpmCalculationTimeMs >= RPM_CALCULATION_PERIOD_MS) {

    // Safely snapshot pulse count
    uint32_t currentPulseCount;
    noInterrupts();
    currentPulseCount = pulseCount;
    interrupts();

    uint32_t deltaPulses = currentPulseCount - lastPulseCount;
    uint32_t dt = nowMs - lastRpmCalculationTimeMs;

    float rpm = 0.0;
    if (dt > 0) {
      rpm = (deltaPulses * 60000.0) / (dt * PULSES_PER_REV);
    }

    // Serial Plotter Output
    Serial.println(rpm);

    // Update tracking variables
    lastPulseCount = currentPulseCount;
    lastRpmCalculationTimeMs = nowMs;
  }
}
