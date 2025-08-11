#include "display.hpp"

#include <SPI.h>
#include <GxEPD2_BW.h>
#include <FreeMonoBold36pt7b.h>
#include <FreeMonoBold30pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

namespace
{
    // Display object for 4.2" 400x300 (GDEH042Z15)
    GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));
    constexpr uint16_t DISPLAY_BUSY_SLEEP = 3;                               // Time to wait in light sleep for display to finish updating
    constexpr uint16_t DISPLAY_FULL_REFRESH_INTERVAL = 200;                  // Number of partial updates before full refresh
    constexpr uint16_t DISPLAY_WIDTH = GxEPD2_420_GDEY042T81::WIDTH_VISIBLE; // Width of the display
    constexpr uint16_t DISPLAY_HEIGHT = GxEPD2_420_GDEY042T81::HEIGHT;       // Height of the display
    constexpr uint16_t DISPLAY_MARGIN = 2;                                   // Margin around the display
    constexpr uint16_t DISPLAY_CENTER_X = DISPLAY_WIDTH / 2;                 // Center X position
    constexpr uint16_t DISPLAY_CENTER_Y = DISPLAY_HEIGHT / 2;                // Center Y position
    constexpr uint16_t UNIT_SPACING = 12;                                    // Spacing between value and unit
    constexpr const char *LABEL_HUMIDITY = "Humidity";
    constexpr const char *LABEL_TEMPERATURE = "Temperature";
    constexpr const char *LABEL_CO2 = "CO2";
    constexpr const char *UNIT_PERCENT = "%";
    constexpr const char *UNIT_CELSIUS = "C";
    constexpr const char *UNIT_PPM = "ppm";

    // Font definitions
    constexpr auto FONT_CO2 = &FreeMonoBold30pt7b;         // Font for CO2 value
    constexpr auto FONT_LABEL = &FreeMonoBold12pt7b;       // Font for labels
    constexpr auto FONT_UNIT = &FreeMonoBold9pt7b;         // Font for units (%, C, ppm)
    constexpr auto FONT_CLOCK = &FreeMonoBold12pt7b;       // Font for clock
    constexpr auto FONT_HUMIDITY = &FreeMonoBold24pt7b;    // Font for humidity
    constexpr auto FONT_TEMPERATURE = &FreeMonoBold24pt7b; // Font for temperature

    // Clock positions (top left corner)
    constexpr uint16_t CLOCK_X = DISPLAY_MARGIN;
    constexpr uint16_t CLOCK_Y = DISPLAY_MARGIN + 18;

    // Battery percentage positions (top right corner)
    constexpr uint16_t BATTERY_PERCENT_X = DISPLAY_WIDTH - DISPLAY_MARGIN - 45;
    constexpr uint16_t BATTERY_PERCENT_Y = DISPLAY_MARGIN + 18;

    // CO2 label and value positions (top half, centered)
    constexpr uint16_t CO2_LABEL_Y = DISPLAY_MARGIN + 18;
    constexpr uint16_t CO2_VALUE_Y = 100;

    // Humidity positions (bottom left quadrant)
    constexpr uint16_t HUMIDITY_LABEL_Y = DISPLAY_CENTER_Y + 18;
    constexpr uint16_t HUMIDITY_VALUE_Y = DISPLAY_HEIGHT - 50;
    constexpr uint16_t HUMIDITY_CENTER_X = DISPLAY_CENTER_X / 2;

    // Temperature positions (bottom right quadrant)
    constexpr uint16_t TEMPERATURE_LABEL_Y = DISPLAY_CENTER_Y + 18;
    constexpr uint16_t TEMPERATURE_VALUE_Y = DISPLAY_HEIGHT - 50;
    constexpr uint16_t TEMPERATURE_CENTER_X = DISPLAY_CENTER_X + (DISPLAY_CENTER_X / 2);

    struct DisplayState
    {
        uint16_t co2 = 0;
        uint16_t temperature = 0;
        uint16_t humidity = 0;
        uint8_t hours = 255;
        uint8_t minutes = 255;
        uint8_t batteryPercent = 0; // 0-100, battery percentage
        bool usbConnected = false;  // USB connection state
        bool error = false;         // Error State
    } currentState, previousState;

    bool showClock = false;                            // Flag for showing clock
    bool fullRefresh = false;                         // Flag for full screen refresh
    char stringBuffer[16];                            // Shared string buffer to avoid repeated allocations
    RTC_DATA_ATTR uint16_t displayRefreshCounter = 0; // Counter for partial updates. Preserved in RTC memory

    void drawBackground()
    {
        // Draw horizontal line dividing the screen into top and bottom halves
        display.drawLine(DISPLAY_MARGIN, DISPLAY_CENTER_Y, DISPLAY_WIDTH - DISPLAY_MARGIN, DISPLAY_CENTER_Y, GxEPD_BLACK);
        // Draw vertical line dividing the bottom half into left and right sections
        display.drawLine(DISPLAY_CENTER_X, DISPLAY_CENTER_Y, DISPLAY_CENTER_X, DISPLAY_HEIGHT - DISPLAY_MARGIN, GxEPD_BLACK);
    }

    // Helper function to draw centered text at given position
    void drawCenteredText(const char *text, const GFXfont *font, uint16_t centerX, uint16_t y)
    {
        display.setFont(font);
        display.setTextColor(GxEPD_BLACK);

        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t x = centerX - (tbw / 2) - tbx;
        display.setCursor(x, y);
        display.print(text);
    }

    // Helper function to draw text with unit
    void drawValueWithUnit(const char *valueText, const char *unitText, const GFXfont *valueFont, uint16_t centerX, uint16_t y)
    {
        display.setFont(valueFont);
        display.setTextColor(GxEPD_BLACK);

        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(valueText, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t valueX = centerX - (tbw / 2) - tbx;
        display.setCursor(valueX, y);
        display.print(valueText);

        // Draw unit to the right of the value
        display.setFont(FONT_UNIT);
        int16_t tbx2, tby2;
        uint16_t tbw2, tbh2;
        display.getTextBounds(unitText, 0, 0, &tbx2, &tby2, &tbw2, &tbh2);

        uint16_t unitX = valueX + tbw + UNIT_SPACING - tbx2;
        display.setCursor(unitX, y);
        display.print(unitText);
    }

    void drawHumidity()
    {
        drawCenteredText(LABEL_HUMIDITY, FONT_LABEL, HUMIDITY_CENTER_X, HUMIDITY_LABEL_Y);
        snprintf(stringBuffer, sizeof(stringBuffer), "%u", currentState.humidity / 100);
        drawValueWithUnit(stringBuffer, UNIT_PERCENT, FONT_HUMIDITY, HUMIDITY_CENTER_X, HUMIDITY_VALUE_Y);
    }

    void drawTemperature()
    {
        drawCenteredText(LABEL_TEMPERATURE, FONT_LABEL, TEMPERATURE_CENTER_X, TEMPERATURE_LABEL_Y);
        snprintf(stringBuffer, sizeof(stringBuffer), "%d.%d", currentState.temperature / 100, currentState.temperature % 10);
        drawValueWithUnit(stringBuffer, UNIT_CELSIUS, FONT_TEMPERATURE, TEMPERATURE_CENTER_X, TEMPERATURE_VALUE_Y);
    }

    void drawClock(const uint8_t hours, const uint8_t minutes)
    {
        snprintf(stringBuffer, sizeof(stringBuffer), "%02d:%02d", hours, minutes);
        display.setFont(FONT_CLOCK);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(CLOCK_X, CLOCK_Y);
        display.print(stringBuffer);
    }

    void drawBatteryPercent()
    {
        display.setFont(FONT_CLOCK);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(BATTERY_PERCENT_X, BATTERY_PERCENT_Y);
        if (currentState.usbConnected)
        {
            display.print("USB");
        }
        else
        {
            snprintf(stringBuffer, sizeof(stringBuffer), "%u", currentState.batteryPercent);
            display.print(stringBuffer);
        }
    }

    void drawCo2()
    {
        drawCenteredText(LABEL_CO2, FONT_LABEL, DISPLAY_CENTER_X, CO2_LABEL_Y);
        snprintf(stringBuffer, sizeof(stringBuffer), "%u", currentState.co2);
        drawValueWithUnit(stringBuffer, UNIT_PPM, FONT_CO2, DISPLAY_CENTER_X, CO2_VALUE_Y);
    }

    void waitBusyFunction()
    {
        Serial.println("Reducing CPU frequency during busy wait");
        setCpuFrequencyMhz(MIN_CPU_FREQ); // Reduce CPU frequency to save power during busy wait

        do
        {
            if (fullRefresh)
            {
                yield();
            }
            else
            {
                esp_sleep_enable_timer_wakeup(DISPLAY_BUSY_SLEEP * 1000);
                esp_light_sleep_start();
            }

        } while (gpio_get_level((gpio_num_t)PIN_BUSY)); // Wait for display to finish updating
        
        Serial.println("Display update complete, restoring CPU frequency");
        setCpuFrequencyMhz(MAX_CPU_FREQ); // Restore CPU frequency after busy wait
    }

    void setupDisplay(bool partial)
    {
        SPI.begin(PIN_SCLK, -1, PIN_MOSI, PIN_CS);
        display.init(0, !partial, 2, false);
        display.epd2.setWaitBusyFunction(waitBusyFunction);
        display.setRotation(0);
    }

    bool getStateChanged()
    {
        // Check basic sensor values (early return for efficiency)
        if (currentState.co2 != previousState.co2 ||
            currentState.temperature != previousState.temperature ||
            currentState.humidity != previousState.humidity ||
            currentState.batteryPercent != previousState.batteryPercent)
        {
            return true;
        }

        // Check clock only if it's enabled and has valid time
        if (showClock &&
            currentState.hours != 255 && currentState.minutes != 255 &&
            (currentState.hours != previousState.hours || currentState.minutes != previousState.minutes))
        {
            return true;
        }

        return false;
    }
};

void updateDisplay(bool partial)
{
    // Force full refresh after a certain number of partial updates
    if (displayRefreshCounter++ >= DISPLAY_FULL_REFRESH_INTERVAL)
    {
        partial = false;
        displayRefreshCounter = 0;
    }

    // Check if any values have changed since the last update
    bool hasChanges = getStateChanged();

    if (!partial || hasChanges)
    {
        setupDisplay(partial);
        display.setFullWindow();
        display.fillScreen(GxEPD_WHITE);
        if (currentState.error)
        {
            drawCenteredText("SENSOR", FONT_CO2, DISPLAY_CENTER_X, DISPLAY_CENTER_Y - 50);
            drawCenteredText("ERROR", FONT_CO2, DISPLAY_CENTER_X, DISPLAY_CENTER_Y);
        }
        else
        {
            drawBackground();

            // Draw all elements if they have valid values
            drawCo2();
            drawTemperature();
            drawHumidity();
        }

        if (showClock)
        {
            drawClock(currentState.hours, currentState.minutes);
        }

        // Always draw battery percentage
        drawBatteryPercent();
        fullRefresh = !partial; // Set flag for full screen refresh
        display.display(partial);
        fullRefresh = false; // Reset flag after display update
        previousState = currentState;
        display.hibernate();
    }
}

void setErrorState(bool error)
{
    currentState.error = error;
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

void setBatteryPercent(const uint8_t percent)
{
    // Clamp battery percentage to valid range (0-100)
    currentState.batteryPercent = (percent > 100) ? 100 : percent;
}

void setUSBConnected(const bool connected)
{
    currentState.usbConnected = connected;
}