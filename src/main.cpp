#include "Display/display.hpp"
#include "Sensor/sensor.hpp"

#include <driver/rtc_io.h>
#include <Arduino.h>

// RTC memory to store persistent data across deep sleep
RTC_DATA_ATTR uint16_t co2Value = 0;       // CO2 value in PPM
RTC_DATA_ATTR uint16_t temperature = 0;    // Temperature in C * 100
RTC_DATA_ATTR uint16_t humidity = 0;       // Humidity in % *
RTC_DATA_ATTR uint16_t currentMinutes = 0; // Current minutes for clock display
RTC_DATA_ATTR uint16_t wakeCount = 0;      // Wake count to track deep sleep cycles

constexpr uint32_t DEEP_SLEEP_DURATION = 60; // Deep sleep duration in seconds (60 seconds = 1 minute)

Sensor sensor;

void initGpio()
{
  pinMode(PIN_RST, OUTPUT); // Set RST pin as output
  pinMode(PIN_DC, OUTPUT);  // Set DC pin as output
  pinMode(PIN_CS, OUTPUT);  // Set CS pin as output

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
}

void goToSleep()
{
  Serial.println("Entering deep sleep...");
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

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting E-Paper Air Monitor...");

  initGpio(); // Initialize GPIO pins
  bool rebootedFromDeepSleep = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
  if (rebootedFromDeepSleep)
  {
    wakeCount++;
  }
  Serial.printf("Wake count: %d\n", wakeCount);

  sensor.begin(); // Initialize the sensor
  if (wakeCount % 5 == 0)
  {
    sensor.update();
  }
  else
  {
    sensor.updateFast();
  }

  Sensor::Measurement measurement = sensor.getMeasurement(); // Get the latest sensor values
  if (co2Value > 0)
  {
    co2Value = measurement.co2; // Store the CO2 value in RTC memory
  }
  temperature = measurement.temperature; // Store the temperature in RTC memory
  humidity = measurement.humidity;       // Store the humidity in RTC memory
  Serial.printf("CO2: %u PPM, Temperature: %.2f C, Humidity: %.2f %%\n", measurement.co2, measurement.temperature / 100.0, measurement.humidity / 100.0);

  enableClock(false);

  // Simulate time advancing
  currentMinutes++;
  if (currentMinutes >= 60)
    currentMinutes = 0;

  setBatteryPercent(75); // Set battery percentage to 75%
  setCo2Value(co2Value);
  setTemperatureValue(temperature);
  setHumidityValue(humidity);
  setTimeValue(12, currentMinutes);

  updateDisplay(rebootedFromDeepSleep);

  goToSleep(); // Enter deep sleep mode
}

void loop()
{
  // This function will never be reached because the ESP32
  // enters deep sleep in setup() and restarts from setup() when it wakes up
  // Deep sleep mode resets the ESP32, so loop() is not executed
}
