#include "display.hpp"
#include <Arduino.h>


void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }
  Serial.println("E-Paper Air Monitor starting...");
  setupDisplay();
}

void loop() {
  updateDisplay();
  delay(10000);
}
