#pragma once
#include <cstdint>

void enterSleepMode(uint16_t duration, bool connected);
uint32_t readBatteryVoltage();
uint8_t getBatteryPercentage(uint32_t batteryVoltage = 0);
