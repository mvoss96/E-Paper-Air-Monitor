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
    constexpr auto FONT_CO2 = &FreeMonoBold36pt7b;                        // Font for CO2 value
    constexpr auto FONT_PPM_LABEL = &FreeMonoBold9pt7b;                   // Font for "ppm" label
    constexpr auto FONT_CLOCK = &FreeMonoBold12pt7b;                      // Font for clock
    constexpr auto FONT_HUMIDITY = &FreeMonoBold12pt7b;                   // Font for humidity
    constexpr auto FONT_TEMPERATURE = &FreeMonoBold12pt7b;                // Font for temperature

    bool sleep_active = false;
    int busy_count = 0;

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
        auto [w, h] = getTextBounds(FONT_CLOCK, "88:88");
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
        auto v = getTextBounds(FONT_CO2, "9999"); // Max 4-digit CO2 value
        auto p = getTextBounds(FONT_PPM_LABEL, "ppm");
        constexpr uint16_t pad = 10, vspace = 25;        // Padding and vertical space between value and label
        uint16_t w = std::max(v.width, p.width) + pad;   // Width: max of value or label + padding
        uint16_t h = v.height + p.height + vspace + pad; // Height: value + label + spacing + padding
        return DisplayRegion{
            static_cast<uint16_t>(DISPLAY_WIDTH - w),   // Right-aligned
            static_cast<uint16_t>(CO2_Y - h / 2 - 15u), // Vertically centered on CO2_Y
            w,                                          // Width with padding
            h                                           // Height with padding
        };
    }();

    // Region for humidity (bottom left)
    const DisplayRegion HUMIDITY_REGION = []
    {
        // Get bounds for "100%" (widest possible humidity string)
        auto [w, h] = getTextBounds(FONT_HUMIDITY, "100%");
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
        auto [w, h] = getTextBounds(FONT_TEMPERATURE, "99.9C");
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

    // Flag for showing clock
    bool showClock = true;

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

    void drawTextInRegion(const DisplayRegion &region, const GFXfont *font, const char *text, const TextAlignment alignment)
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

    void drawHumidity(const uint16_t humidity)
    {
        char humidityStr[5];
        sprintf(humidityStr, "%u%%", humidity);
        drawTextInRegion(HUMIDITY_REGION, FONT_HUMIDITY, humidityStr, TextAlignment::LEFT);
    }

    void drawTemperature(const uint16_t temperature)
    {
        char tempStr[10];
        sprintf(tempStr, "%d.%dC", temperature / 10, temperature % 10);
        drawTextInRegion(TEMPERATURE_REGION, FONT_TEMPERATURE, tempStr, TextAlignment::RIGHT);
    }

    void drawClock(const uint8_t hours, const uint8_t minutes)
    {
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", hours, minutes);
        drawTextInRegion(CLOCK_REGION, FONT_CLOCK, timeStr, TextAlignment::CENTER);
    }

    void drawCo2(const uint16_t co2)
    {
        // Draw CO2 value
        char co2Str[8];
        snprintf(co2Str, sizeof(co2Str), "%u", co2);

        // Calculate position for CO2 value within the region
        display.setFont(FONT_CO2);
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
        display.setFont(FONT_PPM_LABEL);
        display.getTextBounds(ppmStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        // Position ppm label in lower part of the region (right-aligned)
        uint16_t ppmX = CO2_REGION.x + CO2_REGION.w - tbw - tbx;
        uint16_t ppmY = CO2_REGION.y + CO2_REGION.h - 5; // 5px from bottom of region

        display.setCursor(ppmX, ppmY);
        display.print(ppmStr);
    }

    // busyCallback function called during waiting for BUSY to end
    void busyCallback(const void *p)
    {
        digitalWrite(17, HIGH);
        // if (getCpuFrequencyMhz() != MIN_CPU_FREQ)
        // {
        //     setCpuFrequencyMhz(MIN_CPU_FREQ); // Reduce CPU frequency to save power during busy wait
        // }
        if (!sleep_active && busy_count > 10)
        {
            sleep_active = true;

            // gpio_wakeup_enable(static_cast<gpio_num_t>(PIN_BUSY), GPIO_INTR_LOW_LEVEL);
            // esp_sleep_enable_gpio_wakeup();
            // esp_sleep_enable_timer_wakeup(100 * 1000);
            // digitalWrite(17, HIGH);

            // Enter light sleep mode
            // esp_light_sleep_start();

            // digitalWrite(17, LOW);

            // Disable GPIO wakeup after waking up
            // gpio_wakeup_disable(static_cast<gpio_num_t>(PIN_BUSY));
        }
        busy_count++;
    }

    void waitBusyFunction()
    {
        digitalWrite(17, HIGH);
        do
        {
            esp_sleep_enable_timer_wakeup(3000); // 3ms timer wakeup
            esp_light_sleep_start();
        } while (digitalRead(PIN_BUSY) == HIGH); // Wait for display to finish updating
        digitalWrite(17, LOW);
    }

    void setupDisplay(bool isReboot)
    {
        SPI.begin(PIN_SCLK, -1, PIN_MOSI, PIN_CS);
        display.init(0, !isReboot, 2, false);
        display.epd2.setWaitBusyFunction(waitBusyFunction);
        display.setRotation(0);

        // Only do full screen clear and background draw on first boot
        // On reboot after deep sleep, the display content is preserved
        if (!isReboot)
        {
            // Clear screen at start (only on first boot)
            display.setFullWindow();
            display.fillScreen(GxEPD_WHITE);
            if (showBorders)
            {
                drawRegionBorders(); // Draw debug borders only if enabled
            }
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

        // Clear and draw changed regions
        if (co2Changed)
        {
            clearRegion(CO2_REGION);
            drawCo2(currentState.co2);
        }
        if (tempChanged)
        {
            clearRegion(TEMPERATURE_REGION);
            drawTemperature(currentState.temperature);
        }
        if (humidityChanged)
        {
            clearRegion(HUMIDITY_REGION);
            drawHumidity(currentState.humidity);
        }
        if (clockChanged)
        {
            clearRegion(CLOCK_REGION);
            drawClock(currentState.hours, currentState.minutes);
        }
        display.display(true);

        // Update previous state after successful display update
        previousState = currentState;
    }

    // Put display to sleep to save power
    display.hibernate();
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

void enableRegionBorders(const bool show)
{
    showBorders = show;
}

void enableClock(const bool show)
{
    showClock = show;
}
