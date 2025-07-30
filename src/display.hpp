#pragma once
#include <cstdint>

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

