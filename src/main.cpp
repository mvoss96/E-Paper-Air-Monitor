#include "display.hpp"

#include <driver/rtc_io.h>
#include <Arduino.h>

#define DEEP_SLEEP_DURATION_US 10 * 1000000 // Deep sleep duration in microseconds (5 seconds = 5,000,000 microseconds)

// RTC memory to store persistent data across deep sleep
RTC_DATA_ATTR uint16_t co2Value = 800;
RTC_DATA_ATTR uint8_t currentMinutes = 30;

void setup()
{
  Serial.begin(115200);

  // enableRegionBorders(true);
  enableClock(false);

  switch (esp_sleep_get_wakeup_cause())
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
  Serial.flush();  // Make sure all serial output is sent

  rtc_gpio_init((gpio_num_t)PIN_RST);                                     // Initialize the RTC GPIO port
  rtc_gpio_set_direction((gpio_num_t)PIN_RST, RTC_GPIO_MODE_OUTPUT_ONLY); // Set the port to output only mode
  rtc_gpio_hold_dis((gpio_num_t)PIN_RST);                                 // Disable hold before setting the level
  rtc_gpio_set_level((gpio_num_t)PIN_RST, HIGH);                          // Set high/low
  rtc_gpio_hold_en((gpio_num_t)PIN_RST);                                  // Enable hold for the RTC GPIO port

  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION_US); // Configure timer wake up
  esp_deep_sleep_start();                                // Enter deep sleep
}

void loop()
{
  // This function will never be reached because the ESP32
  // enters deep sleep in setup() and restarts from setup() when it wakes up
  // Deep sleep mode resets the ESP32, so loop() is not executed
}
