#include "display.hpp"

#include <algorithm>
#include <utility>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <FreeMonoBold36pt7b.h>
#include <FreeMonoBold30pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

namespace
{
    // Display object for 4.2" 400x300 (GDEH042Z15)
    GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));
    constexpr uint16_t DISPLAY_WIDTH = GxEPD2_420_GDEY042T81::WIDTH_VISIBLE; // Width of the display
    constexpr uint16_t DISPLAY_HEIGHT = GxEPD2_420_GDEY042T81::HEIGHT;       // Height of the display
    constexpr uint16_t DISPLAY_MARGIN = 2;                                   // Margin around the display
    constexpr uint16_t DISPLAY_CENTER_X = DISPLAY_WIDTH / 2;                 // Center X position
    constexpr uint16_t DISPLAY_CENTER_Y = DISPLAY_HEIGHT / 2;                // Center Y position
    constexpr uint16_t UNIT_SPACING = 12;                                    // Spacing between value and unit
    constexpr auto FONT_CO2 = &FreeMonoBold30pt7b;                           // Font for CO2 value
    constexpr auto FONT_LABEL = &FreeMonoBold12pt7b;                         // Font for labels
    constexpr auto FONT_UNIT = &FreeMonoBold9pt7b;                           // Font for units (%, C, ppm)
    constexpr auto FONT_CLOCK = &FreeMonoBold12pt7b;                         // Font for clock
    constexpr auto FONT_HUMIDITY = &FreeMonoBold24pt7b;                      // Font for humidity
    constexpr auto FONT_TEMPERATURE = &FreeMonoBold24pt7b;                   // Font for temperature

    // Clock positions (top left corner)
    constexpr uint16_t CLOCK_X = DISPLAY_MARGIN;
    constexpr uint16_t CLOCK_Y = DISPLAY_MARGIN + 18;

    // CO2 label and value positions (top half, centered)
    constexpr uint16_t CO2_LABEL_Y = DISPLAY_MARGIN + 18;
    constexpr uint16_t CO2_VALUE_Y = 90;

    // Humidity positions (bottom left quadrant)
    constexpr uint16_t HUMIDITY_LABEL_Y = DISPLAY_CENTER_Y + 18;
    constexpr uint16_t HUMIDITY_VALUE_Y = DISPLAY_HEIGHT - 60;
    constexpr uint16_t HUMIDITY_CENTER_X = DISPLAY_CENTER_X / 2;

    // Temperature positions (bottom right quadrant)
    constexpr uint16_t TEMPERATURE_LABEL_Y = DISPLAY_CENTER_Y + 18;
    constexpr uint16_t TEMPERATURE_VALUE_Y = DISPLAY_HEIGHT - 60;
    constexpr uint16_t TEMPERATURE_CENTER_X = DISPLAY_CENTER_X + (DISPLAY_CENTER_X / 2);

    struct DisplayState
    {
        uint16_t co2 = 0;
        uint16_t temperature = 0;
        uint16_t humidity = 0;
        uint8_t hours = 255;
        uint8_t minutes = 255;
    } currentState, previousState;

    bool showClock = true; // Flag for showing clock

    void drawBackground()
    {
        // Draw horizontal line dividing the screen into top and bottom halves
        display.drawLine(DISPLAY_MARGIN, DISPLAY_CENTER_Y, DISPLAY_WIDTH - DISPLAY_MARGIN, DISPLAY_CENTER_Y, GxEPD_BLACK);

        // Draw vertical line dividing the bottom half into left and right sections
        display.drawLine(DISPLAY_CENTER_X, DISPLAY_CENTER_Y, DISPLAY_CENTER_X, DISPLAY_HEIGHT - DISPLAY_MARGIN, GxEPD_BLACK);
    }

    void drawHumidity(const uint16_t humidity)
    {
        // Draw Humidity label
        const char *labelStr = "Humidity";
        display.setFont(FONT_LABEL);
        display.setTextColor(GxEPD_BLACK);

        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(labelStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t labelX = HUMIDITY_CENTER_X - (tbw / 2) - tbx;
        uint16_t labelY = HUMIDITY_LABEL_Y;

        display.setCursor(labelX, labelY);
        display.print(labelStr);

        // Draw humidity value
        char humidityStr[5];
        sprintf(humidityStr, "%u", humidity);

        display.setFont(FONT_HUMIDITY);
        display.getTextBounds(humidityStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t valueX = HUMIDITY_CENTER_X - (tbw / 2) - tbx;
        uint16_t valueY = HUMIDITY_VALUE_Y;

        display.setCursor(valueX, valueY);
        display.print(humidityStr);

        // Draw % unit to the right of the value
        const char *unitStr = "%";
        display.setFont(FONT_UNIT);
        int16_t tbx2, tby2;
        uint16_t tbw2, tbh2;
        display.getTextBounds(unitStr, 0, 0, &tbx2, &tby2, &tbw2, &tbh2);

        uint16_t unitX = valueX + tbw + UNIT_SPACING - tbx2;
        uint16_t unitY = valueY;

        display.setCursor(unitX, unitY);
        display.print(unitStr);
    }

    void drawTemperature(const uint16_t temperature)
    {
        // Draw Temperature label
        const char *labelStr = "Temperature";
        display.setFont(FONT_LABEL);
        display.setTextColor(GxEPD_BLACK);

        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(labelStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t labelX = TEMPERATURE_CENTER_X - (tbw / 2) - tbx;
        uint16_t labelY = TEMPERATURE_LABEL_Y;

        display.setCursor(labelX, labelY);
        display.print(labelStr);

        // Draw temperature value
        char tempStr[10];
        sprintf(tempStr, "%d.%d", temperature / 10, temperature % 10);

        display.setFont(FONT_TEMPERATURE);
        display.getTextBounds(tempStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t valueX = TEMPERATURE_CENTER_X - (tbw / 2) - tbx;
        uint16_t valueY = TEMPERATURE_VALUE_Y;

        display.setCursor(valueX, valueY);
        display.print(tempStr);

        // Draw C unit to the right of the value
        const char *unitStr = "C";
        display.setFont(FONT_UNIT);
        int16_t tbx2, tby2;
        uint16_t tbw2, tbh2;
        display.getTextBounds(unitStr, 0, 0, &tbx2, &tby2, &tbw2, &tbh2);

        uint16_t unitX = valueX + tbw + UNIT_SPACING - tbx2;
        uint16_t unitY = valueY;

        display.setCursor(unitX, unitY);
        display.print(unitStr);
    }

    void drawClock(const uint8_t hours, const uint8_t minutes)
    {
        // Draw clock in the top left
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", hours, minutes);

        display.setFont(FONT_CLOCK);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(CLOCK_X, CLOCK_Y);
        display.print(timeStr);
    }

    void drawCo2(const uint16_t co2)
    {
        // Draw CO2 label centered at the top
        const char *labelStr = "CO2";
        display.setFont(FONT_LABEL);
        display.setTextColor(GxEPD_BLACK);

        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(labelStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t labelX = DISPLAY_CENTER_X - (tbw / 2) - tbx;
        uint16_t labelY = CO2_LABEL_Y;

        display.setCursor(labelX, labelY);
        display.print(labelStr);

        // Draw CO2 value
        char co2Str[8];
        snprintf(co2Str, sizeof(co2Str), "%u", co2);

        display.setFont(FONT_CO2);
        display.getTextBounds(co2Str, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t co2X = DISPLAY_CENTER_X - (tbw / 2) - tbx;
        uint16_t co2Y = CO2_VALUE_Y;

        display.setTextColor(GxEPD_BLACK);
        display.setCursor(co2X, co2Y);
        display.print(co2Str);

        // Draw ppm unit to the right of the value
        const char *unitStr = "ppm";
        display.setFont(FONT_UNIT);
        int16_t tbx2, tby2;
        uint16_t tbw2, tbh2;
        display.getTextBounds(unitStr, 0, 0, &tbx2, &tby2, &tbw2, &tbh2);

        uint16_t unitX = co2X + tbw + UNIT_SPACING - tbx2;
        uint16_t unitY = co2Y;

        display.setCursor(unitX, unitY);
        display.print(unitStr);
    }

    void waitBusyFunction()
    {
        digitalWrite(17, HIGH);
        setCpuFrequencyMhz(MIN_CPU_FREQ); // Reduce CPU frequency to save power during busy wait

        do
        {
            // esp_sleep_enable_timer_wakeup(isSetup ? 2000 : 5000);
            // esp_light_sleep_start();
            yield();
        } while (gpio_get_level((gpio_num_t)PIN_BUSY)); // Wait for display to finish updating

        setCpuFrequencyMhz(MAX_CPU_FREQ); // Restore CPU frequency after busy wait
        digitalWrite(17, LOW);
    }

    void setupDisplay(bool isReboot)
    {
        SPI.begin(PIN_SCLK, -1, PIN_MOSI, PIN_CS);
        display.init(0, !isReboot, 2, false);
        display.epd2.setWaitBusyFunction(waitBusyFunction);
        display.setRotation(0);

        // Only do full screen clear on first boot
        // On reboot after deep sleep, the display content is preserved
        if (!isReboot)
        {
            // Clear screen at start (only on first boot)
            display.setFullWindow();
            display.fillScreen(GxEPD_WHITE);
            display.display();
        }
    }
};

void updateDisplay(bool isReboot)
{
    if (!isReboot)
    {
        // On first boot, we need to initialize the display
        setupDisplay(false);
    }

    // Check if any values have changed since the last update
    bool co2Changed = currentState.co2 != previousState.co2 && currentState.co2 > 0;
    bool tempChanged = currentState.temperature != previousState.temperature && currentState.temperature > 0;
    bool humidityChanged = currentState.humidity != previousState.humidity && currentState.humidity > 0;
    bool clockChanged = showClock && (currentState.hours != previousState.hours || currentState.minutes != previousState.minutes) &&
                        currentState.hours != 255 && currentState.minutes != 255;

    if (!isReboot || co2Changed || tempChanged || humidityChanged || clockChanged)
    {
        setupDisplay(true);
        display.setFullWindow();
        display.fillScreen(GxEPD_WHITE);
        drawBackground();

        // Draw all elements (no need to clear individual regions since we cleared the whole screen)
        if (co2Changed)
        {
            drawCo2(currentState.co2);
        }
        if (tempChanged)
        {
            drawTemperature(currentState.temperature);
        }
        if (humidityChanged)
        {
            drawHumidity(currentState.humidity);
        }
        if (clockChanged)
        {
            drawClock(currentState.hours, currentState.minutes);
        }
        display.display(true);

        // Update previous state after successful display update
        previousState = currentState;
    }

    // Put display to sleep to save power
    display.hibernate();
    display.end();
}

void setCo2Value(const uint16_t co2)
{
    currentState.co2 = co2;
}

void setTemperatureValue(const uint16_t temperature)
{
    currentState.temperature = temperature;
}

void setHumidityValue(const uint16_t humidity)
{
    currentState.humidity = humidity;
}

void setTimeValue(const uint8_t hours, const uint8_t minutes)
{
    currentState.hours = hours;
    currentState.minutes = minutes;
}

void enableClock(const bool show)
{
    showClock = show;
}
