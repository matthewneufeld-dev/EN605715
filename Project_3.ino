/*
  EN.605.715.81.SP26 Software Development for Real-Time Embedded Systems
  Han Nguyen, Matthew Neufeld, Xinyu Xiao | February 22, 2026
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
static const int TACH_PIN = 4;          // IR detector output
static const int IR_EMITTER_PIN = 1;    // IR LED emitter (always ON)

// ======================
// Stepper + Tach Parameters
// ======================
static const float HALFSTEPS_PER_REV = 4096.0f;   // 28BYJ-48 in half-step mode
static const int   PULSES_PER_REV    = 1;

static const uint32_t MIN_PULSE_GAP_MS = 200;            // Debounce filter (µs)
static const uint32_t RPM_CALCULATION_PERIOD_MS = 200;   // RPM sampling interval
static const uint32_t STOPPED_TIMEOUT_MS = 10000;

// ======================
// Half-Step Sequence
// ======================
static const uint8_t SEQ[8][4] = {
  {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
  {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1}
};

// ======================
// Global State
// ======================
volatile uint32_t pulseCount = 0;   // Total tach pulses detected
volatile uint32_t lastPulseMs = 0;  // Timestamp of last valid pulse
volatile uint32_t firstPulseMs = 0; // Timestamp of first valid pulse

int stepIndex = 0;
uint32_t lastRpmCalculationTimeMs = 0;
uint32_t testStartMs = 0;

bool motorEnabled = false;
bool collectingData = false;

float targetRpm = 0.0f;
uint32_t stepDelayUs = 1000;   // default / fallback value

// ======================
// Interrupt Service Routine
// ======================
void IRAM_ATTR tachISR() {
  uint32_t nowMs = millis();

  // Reject pulses that occur too quickly (noise filtering)
  if (nowMs - lastPulseMs >= MIN_PULSE_GAP_MS) {
    if (pulseCount == 0) {
      firstPulseMs = nowMs;
    }
    lastPulseMs = nowMs;
    pulseCount++;
  }
}

// ======================
// Helpers
// ======================
uint32_t rpmToStepDelayUs(float rpm) {
  if (rpm <= 0.0f) return 1000;  // fallback

  // half-steps per second = (rpm * HALFSTEPS_PER_REV) / 60
  float halfStepsPerSec = (rpm * HALFSTEPS_PER_REV) / 60.0f;
  if (halfStepsPerSec < 1.0f) halfStepsPerSec = 1.0f;

  float delayUs = 1000000.0f / halfStepsPerSec;
  if (delayUs < 200.0f) delayUs = 200.0f;  // safety limit

  return (uint32_t)(delayUs + 0.5f);
}

// ======================
// Setup
// ======================
void setup() {
  Serial0.begin(115200);
  delay(2000);
  Serial0.println("Project 3 - Measure and Transmit Fan Speed and Thrust");
  Serial0.println("Authors: Han Nguyen, Matthew Neufeld, Xinyu Xiao");
  Serial0.println("Commands:");
  Serial0.println("  rpm <value>   e.g. rpm 15     -> set desired RPM");
  Serial0.println("  run           -> start motor & collect RPM data");
  Serial0.println("  stop          -> stop motor only");
  Serial0.println("  exit          -> stop motor and data collection/output");
  Serial0.println("Note: Set RPM first, then type 'run' to start collecting RPM data");

  // Configure stepper pins as outputs
  pinMode(STEPPER_PIN1, OUTPUT);
  pinMode(STEPPER_PIN2, OUTPUT);
  pinMode(STEPPER_PIN3, OUTPUT);
  pinMode(STEPPER_PIN4, OUTPUT);
  pinMode(IR_EMITTER_PIN, OUTPUT);

  // IR emitter ON continuously
  digitalWrite(IR_EMITTER_PIN, HIGH);
  pinMode(TACH_PIN, INPUT_PULLUP);

  int intr = digitalPinToInterrupt(TACH_PIN);
  if (intr == -1) {
    Serial0.println("ERROR: TACH_PIN not interrupt-capable");
    while (1) delay(1000);
  }
  attachInterrupt(intr, tachISR, RISING);
}

// ======================
// Main Loop
// ======================
void loop() {
  // ----- Serial Command Handling -----
  static String inputBuffer = "";
  while (Serial0.available()) {
    char c = Serial0.read();
    if (c == '\n' || c == '\r') {
      inputBuffer.trim();
      inputBuffer.toLowerCase();

      if (inputBuffer.startsWith("rpm")) {
        String valStr = inputBuffer.substring(3);
        valStr.trim();
        float newRpm = valStr.toFloat();
        if (newRpm > 0.0f) {
          targetRpm = newRpm;
          stepDelayUs = rpmToStepDelayUs(targetRpm);
          Serial0.print("Target RPM set to: ");
          Serial0.print(targetRpm, 1);
          Serial0.print("  (step delay = ");
          Serial0.print(stepDelayUs);
          Serial0.println(" us)");
        } else {
          Serial0.println("Invalid RPM value");
        }
      }
      else if (inputBuffer == "run") {
        if (targetRpm <= 0.0f) {
          Serial0.println("Please set RPM first (e.g. rpm 20)");
        } else {
          noInterrupts();
          pulseCount = 0;
          firstPulseMs = 0;
          lastPulseMs = 0;
          interrupts();
          testStartMs = millis();
          lastRpmCalculationTimeMs = testStartMs;
          motorEnabled = true;
          collectingData = true;
          stepIndex = 0;
          Serial0.println("RUNNING, collecting RPM data");
          Serial0.println("time_s,rpm");
        }
      }
      else if (inputBuffer == "stop" ) {
        motorEnabled = false;
        Serial0.println("STOP, motor stopped");
      }
      else if (inputBuffer == "exit") {
        motorEnabled = false;
        collectingData = false;
        digitalWrite(STEPPER_PIN1, LOW);
        digitalWrite(STEPPER_PIN2, LOW);
        digitalWrite(STEPPER_PIN3, LOW);
        digitalWrite(STEPPER_PIN4, LOW);
        Serial0.println("EXIT: motor stopped and data collection halted");
      }
      else if (inputBuffer.length() > 0) {
        Serial0.println("Unknown command. Use: rpm <value>, run, stop, exit");
      }

      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }

  // Stepper Motor Control 
  if (motorEnabled) {
    digitalWrite(STEPPER_PIN1, SEQ[stepIndex][0]);
    digitalWrite(STEPPER_PIN2, SEQ[stepIndex][1]);
    digitalWrite(STEPPER_PIN3, SEQ[stepIndex][2]);
    digitalWrite(STEPPER_PIN4, SEQ[stepIndex][3]);
    stepIndex = (stepIndex + 1) % 8;
    delayMicroseconds(stepDelayUs);
  }

  // RPM Calculation & Output 
  if (collectingData) {
    uint32_t nowMs = millis();
    if (nowMs - lastRpmCalculationTimeMs >= RPM_CALCULATION_PERIOD_MS) {
      noInterrupts();
      uint32_t count = pulseCount;
      uint32_t first = firstPulseMs;
      uint32_t last = lastPulseMs;
      interrupts();

      float rpm = 0.0f;
      if (count >= 2) {
        float period_s = (last - first) / 1000.0f;
        if (period_s > 0.0f) {
          rpm = ((count - 1) * 60.0f) / (period_s * PULSES_PER_REV);
        }
      }
      if (last > 0 && (nowMs - last) > STOPPED_TIMEOUT_MS) {
        rpm = 0.0f;
      }

      float elapsed_s = (nowMs - testStartMs) / 1000.0f;

      Serial0.print(elapsed_s, 1);
      Serial0.print(",");
      Serial0.println(rpm, 1);

      lastRpmCalculationTimeMs = nowMs;
    }
  }
}
