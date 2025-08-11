#include "sensor.hpp"
#include "SparkFun_SCD4x_Arduino_Library.h"
#include <Arduino.h>
#include <Preferences.h>

static SCD4x mySensor;
static Preferences preferences;
static constexpr uint16_t SENSOR_SLOW_SLEEP_TIME = 2400; // Sleep interval time for slow sensor updates in milliseconds
static constexpr uint16_t SENSOR_FAST_SLEEP_TIME = 20;   // Sleep interval time for fast sensor updates in milliseconds
RTC_DATA_ATTR static Sensor::Measurement rtcMeasurement; // RTC memory to store sensor measurement data
RTC_DATA_ATTR static Sensor::Config rtcConfig;           // RTC memory to store sensor configuration

void getStoredConfig()
{
    preferences.begin("sensor_config", false);
    rtcConfig.temperatureOffset = preferences.getShort("t_offset", 0);
    rtcConfig.humidityOffset = preferences.getShort("h_offset", 0);
    rtcConfig.frcValue = preferences.getShort("frc_value", 0);

    // Print the loaded configuration
    printf("Loaded Config - Temperature Offset: %d, Humidity Offset: %d, FRC Value: %d\n",
           rtcConfig.temperatureOffset, rtcConfig.humidityOffset, rtcConfig.frcValue);
    preferences.end();
}

static void updateStoredConfig()
{
    preferences.begin("sensor_config", false);
    preferences.putShort("t_offset", rtcConfig.temperatureOffset);
    preferences.putShort("h_offset", rtcConfig.humidityOffset);
    preferences.putShort("frc_value", rtcConfig.frcValue);
    preferences.end();
}

bool Sensor::begin(bool rebooted)
{
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // Begin measurement mode and disable automatic self-calibration
    if (mySensor.begin(false, false, true) == false)
    {
        Serial.println("Error: Sensor not detected!");
        mMeasurement.error = true;
        return false;
    }

    if (!rebooted)
    {
        getStoredConfig();
        scd4x_sensor_type_e sensor;
        mySensor.getFeatureSetVersion(&sensor);
        mySensor.setTemperatureOffset(mConfig.temperatureOffset / 100);
        Serial.printf(
            "Sensor determined to be of type: SCD4%d Temperature offset is: %.2f Sensor altitude is currently: %d Automatic Self Calibration Enabled: %s\n",
            sensor,
            mySensor.getTemperatureOffset(),
            mySensor.getSensorAltitude(),
            mySensor.getAutomaticSelfCalibrationEnabled() ? "true" : "false");
    }

    return true;
}

bool Sensor::updateFast()
{
    Serial.println("Sensor Fast Measurement Requested");
    if (!mySensor.measureSingleShotRHTOnly())
    {
        Serial.println("Error: Fast Single Shot Measurement failed!");
        mMeasurement.error = true;
        return false;
    }

    while (mySensor.getDataReadyStatus() == false)
    {
        Serial.print(".");
        esp_sleep_enable_timer_wakeup(SENSOR_FAST_SLEEP_TIME * 1000);
        esp_light_sleep_start();
    }

    mMeasurement.temperature = mySensor.getTemperature() * 100;
    mMeasurement.humidity = mySensor.getHumidity() * 100;
    printMeasurement();
    return true;
}

bool Sensor::update()
{
    Serial.print("Sensor Measurement Requested ");
    if (!mySensor.measureSingleShot())
    {
        Serial.println("Error: Single Shot Measurement failed!");
        mMeasurement.error = true;
        return false;
    }

    while (mySensor.getDataReadyStatus() == false)
    {
        Serial.print(".");
        esp_sleep_enable_timer_wakeup(SENSOR_SLOW_SLEEP_TIME * 1000);
        esp_light_sleep_start();
    }
    Serial.println();

    mMeasurement.co2 = mySensor.getCO2();
    mMeasurement.temperature = mySensor.getTemperature() * 100;
    mMeasurement.humidity = mySensor.getHumidity() * 100;
    printMeasurement();
    return true;
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

void Sensor::printMeasurement() const
{
    Serial.printf("CO2: %d, Temperature: %d.%02d, Humidity: %d.%02d\n",
                  mMeasurement.co2,
                  mMeasurement.temperature / 100, abs(mMeasurement.temperature % 100),
                  mMeasurement.humidity / 100, abs(mMeasurement.humidity % 100));
}
