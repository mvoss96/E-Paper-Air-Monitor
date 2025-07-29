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
        uint16_t firstLineY = 2 * DISPLAY_MARGIN + 18;
        uint16_t secondLineY = DISPLAY_HEIGHT - 2 * DISPLAY_MARGIN - 18;

        // Draw the two horizontal lines
        display.drawLine(DISPLAY_MARGIN, firstLineY, DISPLAY_WIDTH - DISPLAY_MARGIN, firstLineY, GxEPD_BLACK);
        display.drawLine(DISPLAY_MARGIN, secondLineY, DISPLAY_WIDTH - DISPLAY_MARGIN, secondLineY, GxEPD_BLACK);
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
        int wholePart = temperature / 10;
        int decimalPart = temperature % 10;
        sprintf(tempStr, "%d.%dC", wholePart, decimalPart);

        // Get text bounds to calculate width for right alignment
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(tempStr, 0, 0, &tbx, &tby, &tbw, &tbh);

        // Position text right-aligned with margin
        uint16_t x = DISPLAY_WIDTH - DISPLAY_MARGIN - tbw;
        uint16_t y = DISPLAY_HEIGHT - DISPLAY_MARGIN;

        display.setCursor(x, y);
        display.print(wholePart);
        display.print(".");
        display.print(decimalPart);

        // Get current cursor position to draw degree symbol
        int16_t cursorX = display.getCursorX();
        int16_t cursorY = display.getCursorY();

        display.print("C");
    }

    void drawCo2(uint16_t co2)
    {
        display.setFont(&FreeMonoBold30pt7b);
        display.setTextColor(GxEPD_BLACK);

        char co2Str[8];
        snprintf(co2Str, sizeof(co2Str), "%u", co2);

        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(co2Str, 0, 0, &tbx, &tby, &tbw, &tbh);
        int16_t x = DISPLAY_WIDTH - 10 - tbw - tbx;
        int16_t y = (DISPLAY_HEIGHT / 2) + (tbh / 2);
        display.setCursor(x, y);
        display.print(co2Str);

        display.setFont(&FreeMonoBold9pt7b); // Smaller font
        String ppmStr = "ppm";
        int16_t ppx, ppy;
        uint16_t ppw, pph;
        display.getTextBounds(ppmStr, 0, 0, &ppx, &ppy, &ppw, &pph);
        int16_t px = display.width() - 10 - ppw - ppx;
        int16_t py = y + pph + 5; // 5px below CO2 text
        display.setCursor(px, py);
        display.print(ppmStr);
    }

    void drawClock()
    {
        display.setFont(&FreeMonoBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        // Example time - in real implementation you would get this from RTC
        char timeStr[8];
        sprintf(timeStr, "12:45");
        
        // Get text bounds to calculate width for horizontal centering
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(timeStr, 0, 0, &tbx, &tby, &tbw, &tbh);
        
        // Calculate horizontal center position
        int16_t x = (DISPLAY_WIDTH - tbw) / 2 - tbx;
        
        // Position with margin from top
        int16_t y = DISPLAY_MARGIN + tbh;
        
        display.setCursor(x, y);
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
        // Draw background and other elements here
        drawBackground();
        drawHumidity(52);
        drawTemperature(222);
        drawCo2(800);
        drawClock();
    } while (display.nextPage());
}
