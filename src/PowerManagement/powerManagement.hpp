#pragma once
#include <cstdint>

void enterSleepMode(uint16_t duration, bool connected);
uint32_t readBatteryVoltage();
uint8_t getBatteryPercentage(uint32_t batteryVoltage = 0);

/**
 * @brief Generic smoothing function using fixed-point Exponential Moving Average (EMA)
 * 
 * Implements EMA without floating-point arithmetic, safe for embedded systems.
 * Uses 64-bit arithmetic internally to prevent overflow for all standard integer types.
 * 
 * @param newValue The latest sensor reading or input value
 * @param previousValue The previous smoothed value (use 0 for first reading)
 * @param alphaPercent Smoothing factor as percentage (0-100):
 *                     - Low values (10-20): Heavy smoothing, slow response
 *                     - Medium values (20-40): Balanced smoothing (default: 30)
 *                     - High values (60-90): Light smoothing, fast response
 * 
 * @return Smoothed value of type T
 * 
 * Formula: smoothed = (alpha/100) * newValue + (beta/100) * previousValue
 *          where beta = (100 - alpha)
 */
template <typename T>
T smoothValue(T newValue, T previousValue, uint8_t alphaPercent = 30)
{
    // First reading: no history available, return raw value
    if (previousValue == 0)
    {
        return newValue;
    }
    
    // Edge cases: direct value selection
    if (alphaPercent >= 100)
    {
        return newValue;      // Alpha=100%: no smoothing, use new value only
    }
    if (alphaPercent == 0)
    {
        return previousValue; // Alpha=0%: no update, keep previous value
    }

    // EMA calculation using 64-bit fixed-point arithmetic to prevent overflow
    // Split calculation: alpha_part = newValue * alpha, beta_part = previousValue * beta
    uint64_t alphaPart = (static_cast<uint64_t>(newValue) * alphaPercent);
    uint64_t betaPart = (static_cast<uint64_t>(previousValue) * (100 - alphaPercent));

    // Combine parts and convert back to original type with proper rounding
    // Adding 50 before division by 100 implements "round half up" behavior
    uint64_t smoothed = alphaPart + betaPart;
    T result = static_cast<T>((smoothed + 50) / 100);  // +50 rounds 0.5 up

    return result;
}