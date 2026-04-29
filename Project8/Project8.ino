#include <Arduino_FreeRTOS.h>
#include <Wire.h>
#include "BNO055_support.h"

#define roll_adjustment  -2.69
#define pitch_adjustment  1.06

// -------------------- Function Prototype --------------------
void TaskIMU(void *pvParameters);

// -------------------- BNO055 Structures ----------------------
struct bno055_t myBNO;
struct bno055_euler myEulerData;

// -------------------- Setup ----------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) {;}

  Wire.begin();

  // Initialize IMU
  BNO_Init(&myBNO);
  bno055_set_operation_mode(OPERATION_MODE_NDOF);

  delay(10);

  // Create single IMU task
  xTaskCreate(
    TaskIMU,
    "IMU",
    256,
    NULL,
    1,
    NULL
  );
}

void loop() {
  // Empty - RTOS handles execution
}

// -------------------- IMU Task -------------------------------
void TaskIMU(void *pvParameters) {
  (void) pvParameters;

  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;  // 10 Hz

  for (;;) {
    bno055_read_euler_hrp(&myEulerData);
    float roll  = (float(myEulerData.r) / 16.0) - roll_adjustment;
    float pitch = (float(myEulerData.p) / 16.0) - pitch_adjustment;
    float yaw   = float(myEulerData.h) / 16.0;
    Serial.print("R: ");
    Serial.print(roll, 2);
    Serial.print(",P: ");
    Serial.print(pitch, 2);
    Serial.print(",Y: ");
    Serial.println(yaw, 2);

    vTaskDelay(xDelay);
  }
}