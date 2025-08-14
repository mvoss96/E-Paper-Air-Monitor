#pragma once
#include <stdint.h>

namespace BTHOME
{
    // https://bthome.io/format/
    enum
    {
        BATTERY_UINT8 = 0x01,
        TEMPERATURE_UINT16 = 0x02,
        HUMIDITY_UINT16 = 0x03,
        VOLTAGE_UINT16 = 0x0C,
        CARBON_DIOXIDE_UINT16 = 0x12
    };
    constexpr uint16_t SERVICE_UUID = 0xFCD2;
}

void bleInit();
void bleStopAdvertising();
void bleUpdatePayload(uint16_t humidity, uint16_t temperature,
                      uint16_t carbonDioxide, uint16_t voltage, uint8_t battery);
