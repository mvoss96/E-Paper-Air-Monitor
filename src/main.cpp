#include "display.hpp"
#include <Arduino.h>

#define DEEP_SLEEP_DURATION_US 10 * 1000000 // Deep sleep duration in microseconds (5 seconds = 5,000,000 microseconds)
#define DEBUG_LED_PIN 15                    // Debug LED pin

// RTC memory to store persistent data across deep sleep
RTC_DATA_ATTR uint16_t co2Value = 800;
RTC_DATA_ATTR uint8_t currentMinutes = 30;

void setup()
{
  Serial.begin(115200);

  // Initialize debug LED and turn it on immediately
  // pinMode(DEBUG_LED_PIN, OUTPUT);
  // digitalWrite(DEBUG_LED_PIN, HIGH); // Turn LED on to indicate ESP is awake

  // Print wake up reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  // Debug-Rahmen einschalten
  // enableRegionBorders(true);

  enableClock(false);

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_TIMER:
    setupDisplay(true);
    break;
  default:
    setupDisplay(false);
    break;
  }

  // Simulate changing CO2 value
  co2Value += 10;
  if (co2Value > 1200)
    co2Value = 400;

  // Simulate time advancing
  currentMinutes++;
  if (currentMinutes >= 60)
    currentMinutes = 0;

  setCo2Value(co2Value);
  setTemperatureValue(234);
  setHumidityValue(45);
  setTimeValue(12, currentMinutes);
  updateDisplay(); // Update the display

  // digitalWrite(DEBUG_LED_PIN, LOW); // Turn off debug LED before going to sleep
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION_US); // Configure timer wake up
  Serial.flush();                                        // Make sure all serial output is sent
  esp_deep_sleep_start();                                // Enter deep sleep
}

void loop()
{
  // This function will never be reached because the ESP32
  // enters deep sleep in setup() and restarts from setup() when it wakes up
  // Deep sleep mode resets the ESP32, so loop() is not executed
}
