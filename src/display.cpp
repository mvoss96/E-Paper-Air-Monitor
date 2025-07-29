#include <GxEPD2_BW.h>
#include <FreeMonoBold36pt7b.h>
#include <FreeMonoBold30pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <SPI.h>
#include <algorithm>
#include <utility>

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
    constexpr uint16_t CLOCK_Y = DISPLAY_MARGIN + 18;                     // Clock baseline Y position
    constexpr uint16_t BOTTOM_Y = DISPLAY_HEIGHT - (DISPLAY_MARGIN + 17); // Bottom elements baseline Y

    struct DisplayRegion
    {
        uint16_t x, y, w, h;
    };

    struct TextBounds
    {
        uint16_t width;
        uint16_t height;
    };

    TextBounds getTextBounds(const GFXfont *font, const char *text)
    {
        display.setFont(font);
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
        return {static_cast<uint16_t>(tbw), static_cast<uint16_t>(tbh)};
    }

    // Region for the clock (centered at the top)
    const DisplayRegion CLOCK_REGION = []
    {
        // Get bounds for "88:88" (widest possible time string)
        auto [w, h] = getTextBounds(&FreeMonoBold12pt7b, "88:88");
        constexpr uint16_t pad = 4; // Padding around the text
        return DisplayRegion{
            static_cast<uint16_t>(DISPLAY_CENTER_X - (w + pad) / 2), // Center horizontally
            0,                                                       // Top of the display
            static_cast<uint16_t>(w + pad),                          // Width with padding
            static_cast<uint16_t>(h + pad)                           // Height with padding
        };
    }();

    // Region for CO2 value and "ppm" label (right side, vertically centered)
    const DisplayRegion CO2_REGION = []
    {
        // Get bounds for CO2 value and "ppm" label
        auto v = getTextBounds(&FreeMonoBold30pt7b, "9999"); // Max 4-digit CO2 value
        auto p = getTextBounds(&FreeMonoBold9pt7b, "ppm");
        constexpr uint16_t pad = 10, vspace = 25;        // Padding and vertical space between value and label
        uint16_t w = std::max(v.width, p.width) + pad;   // Width: max of value or label + padding
        uint16_t h = v.height + p.height + vspace + pad; // Height: value + label + spacing + padding
        return DisplayRegion{
            static_cast<uint16_t>(DISPLAY_WIDTH - w),  // Right-aligned
            static_cast<uint16_t>(CO2_Y - h / 2 - 15u), // Vertically centered on CO2_Y
            w,                                         // Width with padding
            h                                          // Height with padding
        };
    }();

    // Region for humidity (bottom left)
    const DisplayRegion HUMIDITY_REGION = []
    {
        // Get bounds for "100%" (widest possible humidity string)
        auto [w, h] = getTextBounds(&FreeMonoBold12pt7b, "100%");
        constexpr uint16_t pad = 2; // Padding around the text
        return DisplayRegion{
            DISPLAY_MARGIN,                 // Left margin
            BOTTOM_Y,                       // Baseline Y for bottom elements
            static_cast<uint16_t>(w + pad), // Width with padding
            static_cast<uint16_t>(h + pad)  // Height with padding
        };
    }();

    // Region for temperature (bottom right)
    const DisplayRegion TEMPERATURE_REGION = []
    {
        // Get bounds for "99.9C" (widest possible temperature string)
        auto [w, h] = getTextBounds(&FreeMonoBold12pt7b, "99.9C");
        constexpr uint16_t pad = 2; // Padding around the text
        return DisplayRegion{
            static_cast<uint16_t>(DISPLAY_WIDTH - DISPLAY_MARGIN - w - pad), // Right-aligned
            BOTTOM_Y,                                                        // Baseline Y for bottom elements
            static_cast<uint16_t>(w + pad),                                  // Width with padding
            static_cast<uint16_t>(h + pad)                                   // Height with padding
        };
    }();

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
        // Calculate positions for the two horizontal lines using constexpr positions
        constexpr uint16_t firstLineY = 2 * DISPLAY_MARGIN + 18;
        constexpr uint16_t secondLineY = DISPLAY_HEIGHT - 2 * DISPLAY_MARGIN - 18;

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

    // Generic function to draw text in a region with specified alignment
    enum class TextAlignment
    {
        LEFT,
        CENTER,
        RIGHT
    };

    void drawTextInRegion(const DisplayRegion &region, const GFXfont *font, const char *text, TextAlignment alignment)
    {
        display.setFont(font);
        display.setTextColor(GxEPD_BLACK);

        // Get text bounds
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);

        // Calculate position based on alignment
        uint16_t posX, posY;

        switch (alignment)
        {
        case TextAlignment::LEFT:
            posX = region.x - tbx;
            break;
        case TextAlignment::CENTER:
            posX = region.x + (region.w - tbw) / 2 - tbx;
            break;
        case TextAlignment::RIGHT:
            posX = region.x + region.w - tbw - tbx;
            break;
        }

        // Always center vertically
        posY = region.y + region.h / 2 - tby / 2;

        display.setCursor(posX, posY);
        display.print(text);
    }

    void drawHumidity(uint16_t humidity)
    {
        char humidityStr[5];
        sprintf(humidityStr, "%u%%", humidity);
        drawTextInRegion(HUMIDITY_REGION, &FreeMonoBold12pt7b, humidityStr, TextAlignment::LEFT);
    }

    void drawTemperature(uint16_t temperature)
    {
        char tempStr[10];
        sprintf(tempStr, "%d.%dC", temperature / 10, temperature % 10);
        drawTextInRegion(TEMPERATURE_REGION, &FreeMonoBold12pt7b, tempStr, TextAlignment::RIGHT);
    }

    void drawClock(uint8_t hours, uint8_t minutes)
    {
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", hours, minutes);
        drawTextInRegion(CLOCK_REGION, &FreeMonoBold12pt7b, timeStr, TextAlignment::CENTER);
    }

    void drawCo2(uint16_t co2)
    {
        // Draw CO2 value
        char co2Str[8];
        snprintf(co2Str, sizeof(co2Str), "%u", co2);

        // Calculate position for CO2 value within the region
        display.setFont(&FreeMonoBold30pt7b);
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(co2Str, 0, 0, &tbx, &tby, &tbw, &tbh);

        // Position CO2 value in upper part of the region (right-aligned)
        uint16_t co2X = CO2_REGION.x + CO2_REGION.w - tbw - tbx;
        uint16_t co2Y = CO2_REGION.y + tbh + 5; // 5px from top of region

        display.setTextColor(GxEPD_BLACK);
        display.setCursor(co2X, co2Y);
        display.print(co2Str);

        // Draw ppm label below CO2 value
        const char *ppmStr = "ppm";
        display.setFont(&FreeMonoBold9pt7b);
        display.getTextBounds(ppmStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        // Position ppm label in lower part of the region (right-aligned)
        uint16_t ppmX = CO2_REGION.x + CO2_REGION.w - tbw - tbx;
        uint16_t ppmY = CO2_REGION.y + CO2_REGION.h - 5; // 5px from bottom of region

        display.setCursor(ppmX, ppmY);
        display.print(ppmStr);
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
