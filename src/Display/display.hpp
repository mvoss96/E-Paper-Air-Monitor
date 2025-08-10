#pragma once
#include <cstdint>

void enableClock(bool show);
void updateDisplay(bool partial);

// Functions to set individual values
void setErrorState(bool error);
void setCo2Value(uint16_t co2);
void setTemperatureValue(uint16_t temperature);
void setHumidityValue(uint16_t humidity);
void setTimeValue(uint8_t hours, uint8_t minutes);
void setBatteryPercent(uint8_t percent); // 0-100%, battery percentage
void setUSBConnected(bool connected); // Set USB connection state


