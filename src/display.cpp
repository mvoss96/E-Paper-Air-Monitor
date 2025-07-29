#include <GxEPD2_BW.h>
#include <FreeMonoBold36pt7b.h>
#include <FreeMonoBold30pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <SPI.h>

// Pin definitions
#define PIN_SCLK 4
#define PIN_MOSI 3
#define PIN_CS 5
#define PIN_DC 6
#define PIN_RST 7
#define PIN_BUSY 8

namespace
{
    // Display object for 1.54" 200x200 (GDEH0154D67)
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));
    constexpr uint16_t DISPLAY_WIDTH = GxEPD2_154_D67::WIDTH_VISIBLE;
    constexpr uint16_t DISPLAY_HEIGHT = GxEPD2_154_D67::HEIGHT;
    constexpr uint16_t DISPLAY_MARGIN = 2;
    constexpr uint16_t DISPLAY_CENTER_X = DISPLAY_WIDTH / 2;
    constexpr uint16_t DISPLAY_CENTER_Y = DISPLAY_HEIGHT / 2;
    constexpr uint16_t CO2_Y = DISPLAY_CENTER_Y + 17;

    // Define display regions for partial updates
    struct DisplayRegion
    {
        uint16_t x, y, w, h;
    };

    // Calculate regions based on content positioning
    const DisplayRegion CLOCK_REGION = {DISPLAY_CENTER_X - 35, 0, 70, 20};
    const DisplayRegion CO2_REGION = {DISPLAY_WIDTH - 150, CO2_Y - 40, 150, 72};
    const DisplayRegion HUMIDITY_REGION = {DISPLAY_MARGIN, DISPLAY_HEIGHT - (DISPLAY_MARGIN + 17), 45, 19};
    const DisplayRegion TEMPERATURE_REGION = {DISPLAY_WIDTH - DISPLAY_MARGIN - 85, DISPLAY_HEIGHT - (DISPLAY_MARGIN + 17), 85, 19};

    // Cache current values to detect changes
    struct DisplayState
    {
        uint16_t co2 = 0;
        uint16_t temperature = 0;
        uint16_t humidity = 0;
        uint8_t hours = 255;
        uint8_t minutes = 255;
    } currentState, previousState;

    // Debug flag for showing region borders
    bool showBorders = false;

    void drawBackground()
    {
        // Calculate positions for the two horizontal lines
        const uint16_t firstLineY = 2 * DISPLAY_MARGIN + 18;
        const uint16_t secondLineY = DISPLAY_HEIGHT - 2 * DISPLAY_MARGIN - 18;

        // Draw the two horizontal lines
        display.drawLine(DISPLAY_MARGIN, firstLineY, DISPLAY_WIDTH - DISPLAY_MARGIN, firstLineY, GxEPD_BLACK);
        display.drawLine(DISPLAY_MARGIN, secondLineY, DISPLAY_WIDTH - DISPLAY_MARGIN, secondLineY, GxEPD_BLACK);
    }

    void clearRegion(const DisplayRegion &region)
    {
        display.fillRect(region.x, region.y, region.w, region.h, GxEPD_WHITE);
    }

    template <typename DrawFunction>
    void updatePartialRegion(const DisplayRegion &region, DrawFunction drawFunction)
    {
        display.setPartialWindow(region.x, region.y, region.w, region.h);
        display.firstPage();
        do
        {
            clearRegion(region);
            drawFunction();
            if (showBorders)
            {
                display.drawRect(region.x, region.y, region.w, region.h, GxEPD_BLACK);
            }
        } while (display.nextPage());
    }

    void drawRegionBorders()
    {
        // Draw borders around all regions for debugging
        // CLOCK_REGION
        display.drawRect(CLOCK_REGION.x, CLOCK_REGION.y, CLOCK_REGION.w, CLOCK_REGION.h, GxEPD_BLACK);

        // CO2_REGION
        display.drawRect(CO2_REGION.x, CO2_REGION.y, CO2_REGION.w, CO2_REGION.h, GxEPD_BLACK);

        // HUMIDITY_REGION
        display.drawRect(HUMIDITY_REGION.x, HUMIDITY_REGION.y, HUMIDITY_REGION.w, HUMIDITY_REGION.h, GxEPD_BLACK);

        // TEMPERATURE_REGION
        display.drawRect(TEMPERATURE_REGION.x, TEMPERATURE_REGION.y, TEMPERATURE_REGION.w, TEMPERATURE_REGION.h, GxEPD_BLACK);
    }

    void drawHumidity(uint16_t humidity)
    {
        display.setFont(&FreeMonoBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(DISPLAY_MARGIN, DISPLAY_HEIGHT - DISPLAY_MARGIN);

        // Add leading space for single digit values to align the % symbol
        if (humidity < 10)
        {
            display.print(" ");
        }
        display.print(humidity);
        display.print("%");
    }

    void drawTemperature(uint16_t temperature)
    {
        display.setFont(&FreeMonoBold12pt7b);
        display.setTextColor(GxEPD_BLACK);

        // Create temperature string with decimal
        char tempStr[10];
        sprintf(tempStr, "%d.%dC", temperature / 10, temperature % 10);

        // Get text bounds for right alignment
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(tempStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        // Position and print right-aligned
        display.setCursor(DISPLAY_WIDTH - DISPLAY_MARGIN - tbw, DISPLAY_HEIGHT - DISPLAY_MARGIN);
        display.print(tempStr);
    }

    void drawCo2(uint16_t co2)
    {
        display.setFont(&FreeMonoBold30pt7b);
        display.setTextColor(GxEPD_BLACK);

        char co2Str[8];
        snprintf(co2Str, sizeof(co2Str), "%u", co2);

        // Get bounds and calculate position for CO2 value
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(co2Str, 0, 0, &tbx, &tby, &tbw, &tbh);

        const int16_t co2X = DISPLAY_WIDTH - 10 - tbw - tbx;

        display.setCursor(co2X, CO2_Y);
        display.print(co2Str);

        // Draw ppm label at fixed offset below CO2 value
        display.setFont(&FreeMonoBold9pt7b);
        const char *ppmStr = "ppm";
        display.getTextBounds(ppmStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        // Reuse co2X calculation but adjust for ppm text width, fixed Y offset of 25
        display.setCursor(DISPLAY_WIDTH - 10 - tbw - tbx, CO2_Y + 25);
        display.print(ppmStr);
    }

    void drawClock(uint8_t hours, uint8_t minutes)
    {
        display.setFont(&FreeMonoBold12pt7b);
        display.setTextColor(GxEPD_BLACK);

        // Format time string
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", hours, minutes);

        // Get text bounds for centering
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(timeStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        // Center horizontally and position with top margin
        display.setCursor((DISPLAY_WIDTH - tbw) / 2 - tbx, DISPLAY_MARGIN + tbh);
        display.print(timeStr);
    }
};

void setupDisplay()
{
    SPI.begin(PIN_SCLK, -1, PIN_MOSI, PIN_CS);
    display.init();
    display.setRotation(0);

    // Clear screen at start
    display.setFullWindow();
    display.firstPage();
    do
    {
        display.fillScreen(GxEPD_WHITE);
        drawBackground();
        if (showBorders)
        {
            drawRegionBorders(); // Draw debug borders only if enabled
        }
    } while (display.nextPage());
}

void updateDisplay()
{
    // Check for changes and update only changed regions
    if (currentState.co2 != previousState.co2 && currentState.co2 > 0)
    {
        updatePartialRegion(CO2_REGION, [=]()
                            { drawCo2(currentState.co2); });
        previousState.co2 = currentState.co2;
    }

    if (currentState.temperature != previousState.temperature && currentState.temperature > 0)
    {
        updatePartialRegion(TEMPERATURE_REGION, [=]()
                            { drawTemperature(currentState.temperature); });
        previousState.temperature = currentState.temperature;
    }

    if (currentState.humidity != previousState.humidity && currentState.humidity > 0)
    {
        updatePartialRegion(HUMIDITY_REGION, [=]()
                            { drawHumidity(currentState.humidity); });
        previousState.humidity = currentState.humidity;
    }

    if ((currentState.hours != previousState.hours || currentState.minutes != previousState.minutes) &&
        currentState.hours != 255 && currentState.minutes != 255)
    {
        updatePartialRegion(CLOCK_REGION, [=]()
                            { drawClock(currentState.hours, currentState.minutes); });
        previousState.hours = currentState.hours;
        previousState.minutes = currentState.minutes;
    }
}

void setCo2Value(uint16_t co2)
{
    currentState.co2 = co2;
}

void setTemperatureValue(uint16_t temperature)
{
    currentState.temperature = temperature;
}

void setHumidityValue(uint16_t humidity)
{
    currentState.humidity = humidity;
}

void setTimeValue(uint8_t hours, uint8_t minutes)
{
    currentState.hours = hours;
    currentState.minutes = minutes;
}

void showRegionBorders(bool show)
{
    showBorders = show;
}
