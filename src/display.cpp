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
    constexpr uint16_t UNIT_SPACING = 12;                                    // Spacing between value and unit
    constexpr uint16_t DISPLAY_CENTER_X = DISPLAY_WIDTH / 2;                 // Center X position
    constexpr uint16_t DISPLAY_CENTER_Y = DISPLAY_HEIGHT / 2;                // Center Y position
    constexpr uint16_t CO2_Y = DISPLAY_CENTER_Y + 17;                        // CO2 value baseline Y position
    constexpr uint16_t CLOCK_Y = DISPLAY_MARGIN + 18;                        // Clock baseline Y position
    constexpr uint16_t BOTTOM_Y = DISPLAY_HEIGHT - (DISPLAY_MARGIN + 34);    // Bottom elements baseline Y
    constexpr auto FONT_CO2 = &FreeMonoBold30pt7b;                           // Font for CO2 value
    constexpr auto FONT_LABEL = &FreeMonoBold12pt7b;                         // Font for labels
    constexpr auto FONT_UNIT = &FreeMonoBold9pt7b;                           // Font for units (%, C, ppm)
    constexpr auto FONT_CLOCK = &FreeMonoBold12pt7b;                         // Font for clock
    constexpr auto FONT_HUMIDITY = &FreeMonoBold24pt7b;                      // Font for humidity
    constexpr auto FONT_TEMPERATURE = &FreeMonoBold24pt7b;                   // Font for temperature

    bool sleep_active = false;
    int busy_count = 0;
    bool isSetup = false;

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

    // Region for the clock (top left)
    const DisplayRegion CLOCK_REGION = []
    {
        // Get bounds for "88:88" (widest possible time string)
        auto [w, h] = getTextBounds(FONT_CLOCK, "88:88");
        constexpr uint16_t pad = 4; // Padding around the text
        return DisplayRegion{
            DISPLAY_MARGIN,                     // Left margin
            DISPLAY_MARGIN,                     // Top margin
            static_cast<uint16_t>(w + pad),     // Width with padding
            static_cast<uint16_t>(h + pad)      // Height with padding
        };
    }();

    // Region for CO2 value and "ppm" label (entire top half)
    const DisplayRegion CO2_REGION = []
    {
        // Get bounds for CO2 value and "ppm" label
        auto v = getTextBounds(FONT_CO2, "9999"); // Max 4-digit CO2 value
        auto p = getTextBounds(FONT_LABEL, "ppm");
        constexpr uint16_t pad = 10, vspace = 25;        // Padding and vertical space between value and label
        uint16_t w = DISPLAY_WIDTH - 2 * DISPLAY_MARGIN; // Full width minus margins
        uint16_t h = DISPLAY_CENTER_Y - DISPLAY_MARGIN;  // Top half height
        return DisplayRegion{
            DISPLAY_MARGIN, // Left margin
            DISPLAY_MARGIN, // Top margin
            w,              // Full width
            h               // Top half height
        };
    }();

    // Region for humidity (bottom left quadrant, centered)
    const DisplayRegion HUMIDITY_REGION = []
    {
        // Get bounds for "99%" (widest possible humidity string)
        auto [w, h] = getTextBounds(FONT_HUMIDITY, "99%");
        constexpr uint16_t pad = 4; // Padding around the text
        // Bottom left quadrant: x from DISPLAY_MARGIN to DISPLAY_CENTER_X, y from DISPLAY_CENTER_Y to DISPLAY_HEIGHT-DISPLAY_MARGIN
        uint16_t quadrant_width = DISPLAY_CENTER_X - DISPLAY_MARGIN;
        uint16_t quadrant_height = DISPLAY_HEIGHT - DISPLAY_CENTER_Y - DISPLAY_MARGIN;
        return DisplayRegion{
            static_cast<uint16_t>(DISPLAY_MARGIN + (quadrant_width - w - pad) / 2),    // Centered horizontally in left quadrant
            static_cast<uint16_t>(DISPLAY_CENTER_Y + (quadrant_height - h - pad) / 2), // Centered vertically in bottom quadrant
            static_cast<uint16_t>(w + pad),                                            // Width with padding
            static_cast<uint16_t>(h + pad)                                             // Height with padding
        };
    }();

    // Region for temperature (bottom right quadrant, centered)
    const DisplayRegion TEMPERATURE_REGION = []
    {
        // Get bounds for "99.9C" (widest possible temperature string)
        auto [w, h] = getTextBounds(FONT_TEMPERATURE, "99.9C");
        constexpr uint16_t pad = 4; // Padding around the text
        // Bottom right quadrant: x from DISPLAY_CENTER_X to DISPLAY_WIDTH-DISPLAY_MARGIN, y from DISPLAY_CENTER_Y to DISPLAY_HEIGHT-DISPLAY_MARGIN
        uint16_t quadrant_width = DISPLAY_WIDTH - DISPLAY_CENTER_X - DISPLAY_MARGIN;
        uint16_t quadrant_height = DISPLAY_HEIGHT - DISPLAY_CENTER_Y - DISPLAY_MARGIN;
        return DisplayRegion{
            static_cast<uint16_t>(DISPLAY_CENTER_X + (quadrant_width - w - pad) / 2),  // Centered horizontally in right quadrant
            static_cast<uint16_t>(DISPLAY_CENTER_Y + (quadrant_height - h - pad) / 2), // Centered vertically in bottom quadrant
            static_cast<uint16_t>(w + pad),                                            // Width with padding
            static_cast<uint16_t>(h + pad)                                             // Height with padding
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
        // Draw horizontal line dividing the screen into top and bottom halves
        display.drawLine(DISPLAY_MARGIN, DISPLAY_CENTER_Y, DISPLAY_WIDTH - DISPLAY_MARGIN, DISPLAY_CENTER_Y, GxEPD_BLACK);

        // Draw vertical line dividing the bottom half into left and right sections
        display.drawLine(DISPLAY_CENTER_X, DISPLAY_CENTER_Y, DISPLAY_CENTER_X, DISPLAY_HEIGHT - DISPLAY_MARGIN, GxEPD_BLACK);
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
        // Draw Humidity label underneath the horizontal line with margin spacing
        const char *labelStr = "Humidity";
        display.setFont(FONT_LABEL);
        display.setTextColor(GxEPD_BLACK);

        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(labelStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t labelX = HUMIDITY_REGION.x + (HUMIDITY_REGION.w - tbw) / 2 - tbx;
        uint16_t labelY = DISPLAY_CENTER_Y + tbh + DISPLAY_MARGIN;

        display.setCursor(labelX, labelY);
        display.print(labelStr);

        // Draw humidity value
        char humidityStr[5];
        sprintf(humidityStr, "%u", humidity);

        display.setFont(FONT_HUMIDITY);
        display.getTextBounds(humidityStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t valueX = HUMIDITY_REGION.x + (HUMIDITY_REGION.w - tbw) / 2 - tbx;
        uint16_t valueY = HUMIDITY_REGION.y + (HUMIDITY_REGION.h / 2) + (tbh / 2) + 10;

        display.setCursor(valueX, valueY);
        display.print(humidityStr);

        // Draw % unit to the right of the value, bottom aligned
        const char *unitStr = "%";
        display.setFont(FONT_UNIT);
        int16_t tbx2, tby2;
        uint16_t tbw2, tbh2;
        display.getTextBounds(unitStr, 0, 0, &tbx2, &tby2, &tbw2, &tbh2);

        uint16_t unitX = valueX + tbw + UNIT_SPACING - tbx2;
        uint16_t unitY = valueY; // Bottom aligned with value

        display.setCursor(unitX, unitY);
        display.print(unitStr);
    }

    void drawTemperature(const uint16_t temperature)
    {
        // Draw Temperature label underneath the horizontal line with margin spacing
        const char *labelStr = "Temperature";
        display.setFont(FONT_LABEL);
        display.setTextColor(GxEPD_BLACK);

        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(labelStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t labelX = TEMPERATURE_REGION.x + (TEMPERATURE_REGION.w - tbw) / 2 - tbx;
        uint16_t labelY = DISPLAY_CENTER_Y + tbh + DISPLAY_MARGIN;

        display.setCursor(labelX, labelY);
        display.print(labelStr);

        // Draw temperature value
        char tempStr[10];
        sprintf(tempStr, "%d.%d", temperature / 10, temperature % 10);

        display.setFont(FONT_TEMPERATURE);
        display.getTextBounds(tempStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t valueX = TEMPERATURE_REGION.x + (TEMPERATURE_REGION.w - tbw) / 2 - tbx;
        uint16_t valueY = TEMPERATURE_REGION.y + (TEMPERATURE_REGION.h / 2) + (tbh / 2) + 10;

        display.setCursor(valueX, valueY);
        display.print(tempStr);

        // Draw C unit to the right of the value, bottom aligned
        const char *unitStr = "C";
        display.setFont(FONT_UNIT);
        int16_t tbx2, tby2;
        uint16_t tbw2, tbh2;
        display.getTextBounds(unitStr, 0, 0, &tbx2, &tby2, &tbw2, &tbh2);

        uint16_t unitX = valueX + tbw + UNIT_SPACING - tbx2;
        uint16_t unitY = valueY; // Bottom aligned with value

        display.setCursor(unitX, unitY);
        display.print(unitStr);
    }

    void drawClock(const uint8_t hours, const uint8_t minutes)
    {
        // Draw clock in the top left region
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", hours, minutes);
        drawTextInRegion(CLOCK_REGION, FONT_CLOCK, timeStr, TextAlignment::LEFT);
    }

    void drawCo2(const uint16_t co2)
    {
        // Draw CO2 label centered at the top of CO2_REGION
        const char *labelStr = "CO2";
        display.setFont(FONT_LABEL);
        display.setTextColor(GxEPD_BLACK);

        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(labelStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        uint16_t labelX = CO2_REGION.x + (CO2_REGION.w - tbw) / 2 - tbx;
        uint16_t labelY = CO2_REGION.y + tbh + DISPLAY_MARGIN;

        display.setCursor(labelX, labelY);
        display.print(labelStr);

        // Draw CO2 value
        char co2Str[8];
        snprintf(co2Str, sizeof(co2Str), "%u", co2);

        // Calculate position for CO2 value within the region (centered horizontally)
        display.setFont(FONT_CO2);
        display.getTextBounds(co2Str, 0, 0, &tbx, &tby, &tbw, &tbh);

        // Position CO2 value in middle part of the region (centered)
        uint16_t co2X = CO2_REGION.x + (CO2_REGION.w - tbw) / 2 - tbx;
        uint16_t co2Y = CO2_REGION.y + (CO2_REGION.h / 2) + (tbh / 2); // Vertically centered

        display.setTextColor(GxEPD_BLACK);
        display.setCursor(co2X, co2Y);
        display.print(co2Str);

        // Draw ppm unit to the right of the value, bottom aligned
        const char *unitStr = "ppm";
        display.setFont(FONT_UNIT);
        int16_t tbx2, tby2;
        uint16_t tbw2, tbh2;
        display.getTextBounds(unitStr, 0, 0, &tbx2, &tby2, &tbw2, &tbh2);

        uint16_t unitX = co2X + tbw + UNIT_SPACING - tbx2;
        uint16_t unitY = co2Y; // Bottom aligned with value

        display.setCursor(unitX, unitY);
        display.print(unitStr);
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

        // Only do full screen clear and background draw on first boot
        // On reboot after deep sleep, the display content is preserved
        if (!isReboot)
        {
            isSetup = true;
            // Clear screen at start (only on first boot)
            display.setFullWindow();
            display.fillScreen(GxEPD_WHITE);
            if (showBorders)
            {
                drawRegionBorders(); // Draw debug borders only if enabled
            }
            display.display();
            isSetup = false; // Reset setup flag after initial clear
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

void enableRegionBorders(const bool show)
{
    showBorders = show;
}

void enableClock(const bool show)
{
    showClock = show;
}
