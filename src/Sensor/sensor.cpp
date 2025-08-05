#include "sensor.hpp"
#include "SparkFun_SCD4x_Arduino_Library.h"
#include <Arduino.h>
#include <Preferences.h>

static SCD4x mySensor;
static Preferences preferences;

static Sensor::Config getStoredConfig()
{
    preferences.begin("sensor_config", false);
    Sensor::Config config{};
    config.temperatureOffset = preferences.getShort("t_offset", 0);
    config.humidityOffset = preferences.getShort("h_offset", 0);
    config.frcValue = preferences.getShort("frc_value", 0);

    // Print the loaded configuration
    printf("Loaded Config - Temperature Offset: %d, Humidity Offset: %d, FRC Value: %d\n",
           config.temperatureOffset, config.humidityOffset, config.frcValue);
    preferences.end();

    return config;
}

static void updateStoredConfig(const Sensor::Config &config)
{
    preferences.begin("sensor_config", false);
    preferences.putShort("t_offset", config.temperatureOffset);
    preferences.putShort("h_offset", config.humidityOffset);
    preferences.putShort("frc_value", config.frcValue);
    preferences.end();
}

bool Sensor::begin()
{
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    mConfig = getStoredConfig();

    if (mySensor.begin(false, false) == false) // Begin measurement mode and disable automatic self-calibration
    {
        Serial.println("Error: Sensor not detected!");
        mMeasurement.error = true;
        return false;
    }

    scd4x_sensor_type_e sensor;
    bool success = mySensor.getFeatureSetVersion(&sensor);
    printf("Sensor determined to be of type: SCD4%d\n", sensor);

    mySensor.setTemperatureOffset(mConfig.temperatureOffset / 100);
    printf("Temperature offset is: %.2f\n", mySensor.getTemperatureOffset());
    printf("Sensor altitude is currently: %d\n", mySensor.getSensorAltitude());
    printf("Automatic Self Calibration Enabled: %s\n", mySensor.getAutomaticSelfCalibrationEnabled() ? "true" : "false");
    return true;
}

bool Sensor::updateFast()
{
    Serial.println("Sensor RH Measurement Requested");
    if (!mySensor.measureSingleShotRHTOnly())
    {
        Serial.println("Error: RH Single Shot Measurement failed!");
        mMeasurement.error = true;
        return false;
    }

    while (mySensor.getDataReadyStatus() == false)
    {
        Serial.print(".");
        esp_sleep_enable_timer_wakeup(20 * 1000);
        esp_light_sleep_start();
    }

    if (mySensor.readMeasurement())
    {
        mMeasurement.temperature = mySensor.getTemperature() * 100;
        mMeasurement.humidity = mySensor.getHumidity() * 100;
        return true;
    }
    Serial.println("No new measurement data available.");

    return false;
}

bool Sensor::update()
{
    Serial.println("Sensor Measurement Requested");
    if (!mySensor.measureSingleShot())
    {
        Serial.println("Error: Single Shot Measurement failed!");
        mMeasurement.error = true;
        return false;
    }

    while (mySensor.getDataReadyStatus() == false)
    {
        Serial.print(".");
        esp_sleep_enable_timer_wakeup(1000 * 1000);
        esp_light_sleep_start();
    }

    if (mySensor.readMeasurement())
    {
        mMeasurement.co2 = mySensor.getCO2();
        mMeasurement.temperature = mySensor.getTemperature() * 100;
        mMeasurement.humidity = mySensor.getHumidity() * 100;
        return true;
    }
    Serial.println("No new measurement data available.");

    return false;
}

Sensor::Config Sensor::getConfig() const
{
    return mConfig;
}

Sensor::Measurement Sensor::getMeasurement() const
{
    return mMeasurement;
}

void Sensor::startFRC()
{
    float correction;
    printf("Starting FRC with value: %d\n", mConfig.frcValue);
    mySensor.performForcedRecalibration(mConfig.frcValue, &correction);
    mySensor.persistSettings();
    printf("FRC completed. Correction value: %.2f\n", correction);
    delay(500);
    ESP.restart();
}