#include <Wire.h>
#include "SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h"

NAU7802 myScale; // Create an instance of the NAU7802 scale

// ==========================================
// Custom I2C Pins 
// (Uncomment and change these if you are using an ESP32/ESP8266 and need manual pins)
// ==========================================
#define I2C_SDA_PIN 42 
#define I2C_SCL_PIN 41 

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("NAU7802 Load Cell Test");

  // Initialize I2C 
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); // Uncomment if using custom pins
  // Wire.begin();

  // Attempt to connect to the NAU7802
  if (myScale.begin() == false) {
    Serial.println("Error: NAU7802 not detected. Check your I2C wiring!");
    while (1);
  }
  Serial.println("NAU7802 Connected Successfully.");

  // Tare the scale (Establish a zero point)
  Serial.println("Taring scale... Please ensure the scale is empty.");
  delay(2000); // Wait 2 seconds for the scale to physically settle
  myScale.calculateZeroOffset();
  Serial.println("Tare complete!");

  // -------------------------------------------------------------
  // CALIBRATION FACTOR:
  // This is a placeholder value. Every load cell is different. 
  // To get real weight (e.g., grams or lbs), place a known weight 
  // on the scale, look at the raw reading, and adjust this number 
  // until the output matches your known weight.
  // -------------------------------------------------------------
  myScale.setCalibrationFactor(1000.0); 
}

void loop() {
  // Check if new data is available from the ADC
  if (myScale.available() == true) {
    
    // 1. Get raw ADC value (Great for verifying if the load cell works at all)
    long rawData = myScale.getReading();
    
    // 2. Get calculated weight (Requires adjusting the Calibration Factor in setup)
    float weight = myScale.getWeight(); 

    // Print to Serial Monitor
    Serial.print("Raw Data: ");
    Serial.print(rawData);
    Serial.print(" \t | Calculated Weight: ");
    Serial.println(weight, 2); 
  }
  
  delay(100); // Run at roughly 10Hz
}