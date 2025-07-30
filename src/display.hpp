#pragma once
#include <cstdint>

// Pin definitions
#define PIN_SCLK 4
#define PIN_MOSI 3
#define PIN_CS 5
#define PIN_DC 6
#define PIN_RST 7
#define PIN_BUSY 8

void setupDisplay(bool isReboot = false);

// Main update function - only updates changed values
void updateDisplay();

// Functions to set individual values
void setCo2Value(uint16_t co2);
void setTemperatureValue(uint16_t temperature);
void setHumidityValue(uint16_t humidity);
void setTimeValue(uint8_t hours, uint8_t minutes);

// Enable or disable region borders for debugging
void enableRegionBorders(bool show);
// Enable or disable clock display
void enableClock(bool show);
