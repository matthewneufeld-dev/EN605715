#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHTesp.h"

// OLED configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

// Pin configuration
#define DHT_PIN D6      // DHT11 data pin
#define SDA_PIN D2      // I2C SDA for OLED
#define SCL_PIN D1      // I2C SCL for OLED

// Create objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHTesp dht;

void setup() {
  Serial.begin(115200);

  // Initialize I2C for OLED
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize DHT11 sensor
  dht.setup(DHT_PIN, DHTesp::DHT11);

  // Initialize OLED display
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  display.clearDisplay();
}

void loop() {
  // Read temperature and humidity from DHT11
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();

  printToSerial(temperature, humidity);
  updateDisplay(temperature, humidity);

  delay(2000);
}

// Print sensor data to Serial Monitor - For debugging
void printToSerial(float temperature, float humidity) {
  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print(" C  Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
}

// Display sensor data on OLED screen
void updateDisplay(float temperature, float humidity) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ESP8266 Weather");

  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print("T:");
  display.print(temperature, 1);
  display.print("C");

  display.setCursor(0, 45);
  display.print("H:");
  display.print(humidity, 1);
  display.print("%");

  display.display();
}
