#include "ble.hpp"
// #include <ArduinoBLE.h>
#include "NimBLEDevice.h"
#include <NimBLEBeacon.h>

namespace
{
    struct BTHomeData
    {
        uint8_t battery;
        uint16_t humidity;
        uint16_t temperature;
        uint16_t carbonDioxide;
        uint16_t voltage;

        static constexpr size_t payloadSize = 15;

        void toPayload(uint8_t *payload) const
        {
            payload[0] = 0x40; // Flags
            payload[1] = BTHOME::BATTERY_UINT8;
            payload[2] = battery;
            payload[3] = BTHOME::HUMIDITY_UINT16;
            payload[4] = humidity & 0xFF;
            payload[5] = (humidity >> 8) & 0xFF;
            payload[6] = BTHOME::TEMPERATURE_UINT16;
            payload[7] = temperature & 0xFF;
            payload[8] = (temperature >> 8) & 0xFF;
            payload[9] = BTHOME::CARBON_DIOXIDE_UINT16;
            payload[10] = carbonDioxide & 0xFF;
            payload[11] = (carbonDioxide >> 8) & 0xFF;
            payload[12] = BTHOME::VOLTAGE_UINT16;
            payload[13] = voltage & 0xFF;
            payload[14] = (voltage >> 8) & 0xFF;
        }
    };

    BTHomeData bthomeData{};
    constexpr const char *DEVICE_NAME = "AirMonitor";
    uint8_t payload[BTHomeData::payloadSize];
    BLEAdvertising *pAdvertising;
}

void bleInit()
{
    Serial.println("Initializing BLE...");
    BLEDevice::init(DEVICE_NAME);
}

void bleUpdatePayload(uint16_t humidity, uint16_t temperature, uint16_t carbonDioxide, uint16_t voltage, uint8_t battery)
{
    Serial.printf("Updating BLE payload with Humidity: %d, Temperature: %d, CO2: %d, Voltage: %d, Battery: %d\n",
                  humidity, temperature, carbonDioxide, voltage, battery);
    bthomeData.humidity = humidity;
    bthomeData.temperature = temperature;
    bthomeData.carbonDioxide = carbonDioxide;
    bthomeData.voltage = voltage;
    bthomeData.battery = battery;
    bthomeData.toPayload(payload);

    pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    pAdvertising->setServiceData(NimBLEUUID((uint16_t)0xFCD2), std::string((char *)payload, BTHomeData::payloadSize));
    pAdvertising->setConnectableMode(2);      // LE General Discoverable
    pAdvertising->setDiscoverableMode(0);     // BR/EDR Not Supported
    pAdvertising->setAdvertisingInterval(40); // Interval in 0.625ms units -> 25ms
    pAdvertising->setAdvertisingCompleteCallback([](NimBLEAdvertising *)
                                                 { Serial.println("BLE advertising complete"); });
    pAdvertising->start();
}

void bleStopAdvertising()
{
    Serial.println("Stopping BLE advertising...");
    pAdvertising->stop();
}
