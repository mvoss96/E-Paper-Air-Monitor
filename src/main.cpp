#include "display.hpp"

#include <driver/rtc_io.h>
#include <Arduino.h>

// RTC memory to store persistent data across deep sleep
RTC_DATA_ATTR uint16_t co2Value = 800;
RTC_DATA_ATTR uint8_t currentMinutes = 30;

void setup()
{
  Serial.begin(115200);
  pinMode(17, OUTPUT);
  digitalWrite(17, LOW);

  // Release the RST pin hold from deep sleep so the display can use it
  rtc_gpio_init((gpio_num_t)PIN_RST);                                     // Initialize the RTC GPIO port
  rtc_gpio_set_direction((gpio_num_t)PIN_RST, RTC_GPIO_MODE_OUTPUT_ONLY); // Set the port to output only mode
  rtc_gpio_hold_dis((gpio_num_t)PIN_RST);                                 // Disable hold before setting the level

  rtc_gpio_init((gpio_num_t)PIN_DC);                                     // Initialize the DC pin
  rtc_gpio_set_direction((gpio_num_t)PIN_DC, RTC_GPIO_MODE_OUTPUT_ONLY); // Set the port to output only mode
  rtc_gpio_hold_dis((gpio_num_t)PIN_DC);                                 // Disable hold before setting the level

  rtc_gpio_init((gpio_num_t)PIN_CS);                                     // Initialize the CS pin
  rtc_gpio_set_direction((gpio_num_t)PIN_CS, RTC_GPIO_MODE_OUTPUT_ONLY); // Set the port to output only mode
  rtc_gpio_hold_dis((gpio_num_t)PIN_CS);                                 // Disable hold before setting the level

  // enableRegionBorders(true);
  //  enableClock(false);

  // Simulate changing CO2 value
  co2Value += 10;
  if (co2Value > 1200)
    co2Value = 400;

  // Simulate time advancing
  currentMinutes++;
  if (currentMinutes >= 60)
    currentMinutes = 0;

  // Random values for temperature and humidity
  uint16_t temperature = 321; // Example temperature value
  uint16_t humidity = 8;      // Example humidity value

  setBatteryPercent(75); // Set battery percentage to 75%
  setCo2Value(co2Value);
  setTemperatureValue(temperature);
  setHumidityValue(humidity);
  setTimeValue(12, currentMinutes);
  updateDisplay(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);

  Serial.flush(); // Make sure all serial output is sent

  rtc_gpio_set_level((gpio_num_t)PIN_RST, HIGH); // Set HIGH for RST pin
  rtc_gpio_hold_en((gpio_num_t)PIN_RST);         // Enable hold for the RTC GPIO port

  rtc_gpio_set_level((gpio_num_t)PIN_DC, LOW); // Set LOW for DC pin
  rtc_gpio_hold_en((gpio_num_t)PIN_DC);        // Enable hold for the DC pin

  rtc_gpio_set_level((gpio_num_t)PIN_CS, LOW); // Set LOW for CS pin
  rtc_gpio_hold_en((gpio_num_t)PIN_CS);        // Enable hold for the

  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION * 1000000); // Configure timer wake up
  esp_deep_sleep_start();                                       // Enter deep sleep
}

void loop()
{
  // This function will never be reached because the ESP32
  // enters deep sleep in setup() and restarts from setup() when it wakes up
  // Deep sleep mode resets the ESP32, so loop() is not executed
}
