#include "powerManagement.hpp"

#include "driver/rtc_io.h"
#include <Arduino.h>

static constexpr uint32_t BAT_EMPTY_VOLTAGE = 3000;      // Empty battery voltage in mV
static constexpr uint32_t BAT_FULL_VOLTAGE = 4150;       // Full battery voltage in mV
static constexpr float BAT_VOLTAGE_DIVIDER_RATIO = 4.38; // Voltage divider ratio for battery voltage measurement

void enterSleepMode(uint16_t duration, bool connected)
{
    Serial.printf("Entering deep sleep for %d seconds. Enabling wakeup for USB %s...\n", duration, connected ? "disconnection" : "connection");
    Serial.flush(); // Make sure all serial output is sent

    rtc_gpio_set_level((gpio_num_t)PIN_RST, HIGH); // Set HIGH for RST pin
    rtc_gpio_hold_en((gpio_num_t)PIN_RST);         // Enable hold for the RTC GPIO port

    rtc_gpio_set_level((gpio_num_t)PIN_DC, LOW); // Set LOW for DC pin
    rtc_gpio_hold_en((gpio_num_t)PIN_DC);        // Enable hold for the DC pin

    rtc_gpio_set_level((gpio_num_t)PIN_CS, LOW); // Set LOW for CS pin
    rtc_gpio_hold_en((gpio_num_t)PIN_CS);        // Enable hold for the CS pin

    esp_deep_sleep_disable_rom_logging(); // Disable ROM logging to save power

    // Check current USB state and configure wakeup accordingly
    if (connected)
    {
        // USB is connected, wake up when disconnected (HIGH→LOW)
        esp_sleep_enable_ext1_wakeup(1ULL << PIN_USB_DETECT, ESP_EXT1_WAKEUP_ANY_LOW);
    }
    else
    {
        // USB is not connected, wake up when connected (LOW→HIGH)
        esp_sleep_enable_ext1_wakeup(1ULL << PIN_USB_DETECT, ESP_EXT1_WAKEUP_ANY_HIGH);
    }

    esp_sleep_enable_timer_wakeup(duration * 1000000); // Configure timer wake up

    esp_deep_sleep_start(); // Enter deep sleep
}

uint32_t readBatteryVoltage()
{
    return analogReadMilliVolts(PIN_BAT_VOLTAGE) * BAT_VOLTAGE_DIVIDER_RATIO;
}

uint8_t getBatteryPercentage(uint32_t batteryVoltage)
{
    if (batteryVoltage == 0)
    {
        batteryVoltage = readBatteryVoltage(); // Read battery voltage if not provided
    }
    return map(batteryVoltage, BAT_EMPTY_VOLTAGE, BAT_FULL_VOLTAGE, 0, 100);
}
