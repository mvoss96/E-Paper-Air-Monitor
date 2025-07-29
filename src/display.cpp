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

    void drawBackground()
    {
        // Calculate positions for the two horizontal lines
        const uint16_t firstLineY = 2 * DISPLAY_MARGIN + 18;
        const uint16_t secondLineY = DISPLAY_HEIGHT - 2 * DISPLAY_MARGIN - 18;

        // Draw the two horizontal lines
        display.drawLine(DISPLAY_MARGIN, firstLineY, DISPLAY_WIDTH - DISPLAY_MARGIN, firstLineY, GxEPD_BLACK);
        display.drawLine(DISPLAY_MARGIN, secondLineY, DISPLAY_WIDTH - DISPLAY_MARGIN, secondLineY, GxEPD_BLACK);

        // Draw ppm text
        display.setFont(&FreeMonoBold9pt7b);
        const char* ppmStr = "ppm";
        int16_t ppx, ppy;
        uint16_t ppw, pph;
        display.getTextBounds(ppmStr, 0, 0, &ppx, &ppy, &ppw, &pph);
        
        display.setCursor(DISPLAY_WIDTH - 10 - ppw - ppx, DISPLAY_HEIGHT / 2 + pph + 25);
        display.print(ppmStr);
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

        // Get bounds and center position
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(co2Str, 0, 0, &tbx, &tby, &tbw, &tbh);
        
        display.setCursor(DISPLAY_WIDTH - 10 - tbw - tbx, (DISPLAY_HEIGHT / 2) + (tbh / 2));
        display.print(co2Str);
    }

    void drawClock()
    {
        display.setFont(&FreeMonoBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        // Example time - in real implementation you would get this from RTC
        const char* timeStr = "12:45";
        
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
    } while (display.nextPage());
}

void updateDisplay()
{
    display.setFullWindow();
    display.firstPage();
    do
    {
        drawBackground();
        drawHumidity(52);
        drawTemperature(222);
        drawCo2(800);
        drawClock();
    } while (display.nextPage());
}
