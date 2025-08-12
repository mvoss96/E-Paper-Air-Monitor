#include "Display/display.hpp"
#include "Sensor/sensor.hpp"
#include "PowerManagement/powerManagement.hpp"

#include <driver/rtc_io.h>
#include <Arduino.h>

// RTC memory to store persistent data across deep sleep
struct RtcData
{
  uint16_t co2Value = 0;         // CO2 value in PPM
  uint16_t humidityValue = 0;    // Humidity value in % * 100
  uint16_t temperatureValue = 0; // Temperature value in C * 100
  uint16_t wakeCount = 0;        // Wake count to track deep sleep cycles
};

RTC_DATA_ATTR RtcData rtcData{};
Sensor sensor;

void initGpio()
{
  pinMode(PIN_RST, OUTPUT);        // Set RST pin as output
  pinMode(PIN_DC, OUTPUT);         // Set DC pin as output
  pinMode(PIN_CS, OUTPUT);         // Set CS pin as output
  pinMode(PIN_BAT_VOLTAGE, INPUT); // Set Battery Voltage pin as input
  pinMode(PIN_USB_DETECT, INPUT);  // Set USB Detect pin as input

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

bool getUsbConnected()
{
  return digitalRead(PIN_USB_DETECT);
}

void batteryMode(bool reboot)
{
  // Read Battery Voltage
  uint32_t batteryVoltage = readBatteryVoltage();
  uint8_t batteryPercent = getBatteryPercentage(batteryVoltage);
  Serial.printf("Battery Voltage: %u mV, Percentage: %u%%\n", batteryVoltage, batteryPercent);

  if (reboot)
  {
    rtcData.wakeCount++;
  }

  if (rtcData.wakeCount % 5 == 0)
  {
    sensor.update(); // Every 5th wake, perform a full sensor update
  }
  else
  {
    sensor.updateFast(); // Fast update for temperature and humidity only
  }

  Sensor::Measurement measurement = sensor.getMeasurement(); // Get the latest sensor values
  if (measurement.co2 > 0)                                   // Update CO2 value in RTC memory
  {
    rtcData.co2Value = measurement.co2;
  }
  rtcData.humidityValue = measurement.humidity;
  rtcData.temperatureValue = measurement.temperature;

  setBatteryPercent(batteryPercent);
  setCo2Value(rtcData.co2Value);
  setErrorState(measurement.error);
  setHumidityValue(rtcData.humidityValue);
  setTemperatureValue(rtcData.temperatureValue);
  setUSBConnected(false);

  updateDisplay(reboot);
  enterSleepMode();
}

void usbMode(bool reboot)
{
  setUSBConnected(true);
  while (getUsbConnected())
  {
    unsigned long startTime = millis();
    sensor.update();
    auto measurement = sensor.getMeasurement();

    setCo2Value(measurement.co2);
    setErrorState(measurement.error);
    setHumidityValue(measurement.humidity);
    setTemperatureValue(measurement.temperature);
    updateDisplay(true);

    // Update RTC
    rtcData.co2Value = measurement.co2;
    rtcData.humidityValue = measurement.humidity;
    rtcData.temperatureValue = measurement.temperature;
    rtcData.wakeCount = 0;

    unsigned long elapsedTime = millis() - startTime;
    while (millis() - startTime < 5000)
    {
      if (!getUsbConnected())
      {
        return;
      }
      delay(100);
    }
  }

  Serial.println("USB disconnected enter Battery Mode");
  batteryMode(true);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n---Starting E-Paper Air Monitor---");

  initGpio(); // Initialize GPIO pins
  bool reboot = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER || esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
  if (reboot)
  {
    Serial.printf("Wake count: %d\n", rtcData.wakeCount);
  }
  else
  {
    Serial.println("First boot, initializing sensor...");
  }
  sensor.begin(reboot);

  if (getUsbConnected())
  {
    Serial.println("USB is connected, entering USB mode...");
    usbMode(reboot);
  }
  else
  {
    Serial.println("USB is not connected, entering battery mode...");
    batteryMode(reboot);
  }
}

void loop()
{
  // This function will never be reached because the ESP32
  // enters deep sleep in setup() and restarts from setup() when it wakes up
  // Deep sleep mode resets the ESP32, so loop() is not executed
}
