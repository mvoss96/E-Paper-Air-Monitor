#pragma once
#include <cstdint>

void setupDisplay();

// Main update function - only updates changed values
void updateDisplay();

// Functions to set individual values
void setCo2Value(uint16_t co2);
void setTemperatureValue(uint16_t temperature);
void setHumidityValue(uint16_t humidity);
void setTimeValue(uint8_t hours, uint8_t minutes);

// Debug function to show region borders
void showRegionBorders(bool show);