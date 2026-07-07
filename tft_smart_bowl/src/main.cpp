#include <Arduino.h>
#include "app/Application.h"

void setup() {
  Serial.begin(115200);
  delay(100);
  Application::getInstance().setup();
}

void loop() {
  Application::getInstance().loop();
}