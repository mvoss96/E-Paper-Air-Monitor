#pragma once
#include <cstdint>

class Sensor
{
public:
    struct Measurement
    {
        uint16_t co2;         // CO2 value in PPM
        uint16_t temperature; // Temperature in C * 100
        uint16_t humidity;    // Humidity in % * 100
        bool error;           // Error flag
    };

    struct Config
    {
        int16_t temperatureOffset; // Temperature offset in C * 100
        int16_t humidityOffset;    // Humidity offset in % * 100
        uint16_t frcValue;         // FRC value
    };

    static constexpr uint16_t NOT_READY_VALUE = 0xFFFF; // Value to indicate sensor is not ready
    Sensor() = default;                                 // Constructor
    bool begin();                                       // Start the sensor and return true if it was detected
    bool update();                                      // Update the sensor values, returns true if new values are available
    Config getConfig() const;                           // Get the current sensor configuration
    Measurement getMeasurement() const;                 // Get the latest measurement values
    void startFRC();                                    // Start the forced recalibration

private:
    // Startup times for different measurements
    static constexpr uint16_t STARTUP_TIME_C = 60;  // Startup time in seconds (CO2)
    static constexpr uint16_t STARTUP_TIME_H = 90;  // Startup time in seconds (Humidity)
    static constexpr uint16_t STARTUP_TIME_T = 120; // Startup time in seconds (Temperature)

    unsigned long mSensorStartupTime = 0; // Sensor startup time
    Measurement mMeasurement{};           // Current measurement values
    Config mConfig{};                     // Sensor configuration
};
