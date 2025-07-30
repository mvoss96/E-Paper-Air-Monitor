#include "display.hpp"
#include <Arduino.h>


void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }
  Serial.println("E-Paper Air Monitor starting...");
  setupDisplay();

  // Debug-Rahmen einschalten
  //showRegionBorders(true);
  
  // Set initial values
  setCo2Value(1234);
  setTemperatureValue(234);
  setHumidityValue(45);
  setTimeValue(12, 30);

}

void loop() {
  static uint16_t co2 = 800;
  static uint8_t minutes = 30;
  
  // Simulate changing CO2 value
  co2 += 10;
  if (co2 > 1200) co2 = 400;
  
  // Simulate time advancing
  minutes++;
  if (minutes >= 60) minutes = 0;

  setCo2Value(co2);
  setTimeValue(12, minutes);
  updateDisplay();

}
