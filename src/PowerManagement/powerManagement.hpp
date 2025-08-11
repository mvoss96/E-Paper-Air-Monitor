#pragma once
#include <cstdint>

void enterSleepMode();
uint32_t readBatteryVoltage();
uint8_t getBatteryPercentage(uint32_t batteryVoltage = 0);
