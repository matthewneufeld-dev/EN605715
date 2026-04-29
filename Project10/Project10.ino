#include <Arduino_FreeRTOS.h>
#include <Wire.h>
#include "BNO055_support.h"

#define roll_adjustment  -2.69
#define pitch_adjustment  1.06
#define ARDUINO_I2C_ADDR 0x08

// -------------------- Function Prototypes --------------------
void TaskIMU(void *pvParameters);
void onPiRequest();

// -------------------- BNO055 Structures ----------------------
struct bno055_t myBNO;
struct bno055_euler myEulerData;

// -------------------- Shared Buffers -------------------------
char i2cBuffer[32] = "R:0.00,P:0.00,Y:0.00";

// -------------------- Setup ----------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) {;}

  // Set Arduino as I2C slave
  Wire.begin(ARDUINO_I2C_ADDR);
  Wire.onRequest(onPiRequest);

  // Initialize IMU
  BNO_Init(&myBNO);
  bno055_set_operation_mode(OPERATION_MODE_NDOF);

  delay(10);

  // Create IMU task
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
    // Build string for I2C
    String temp = "R:";
    temp += String(roll, 2);
    temp += ",P:";
    temp += String(pitch, 2);
    temp += ",Y:";
    temp += String(yaw, 2);
    //Serial.println(i2cBuffer); Uncomment when debugging

    // Critical section: copy string into I2C buffer
    noInterrupts();
    temp.toCharArray(i2cBuffer, sizeof(i2cBuffer));
    interrupts();

    vTaskDelay(xDelay);
  }
}

// -------------------- I2C Request Handler --------------------
void onPiRequest() {
  Wire.write((const uint8_t *)i2cBuffer, sizeof(i2cBuffer));
}