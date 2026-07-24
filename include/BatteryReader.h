#pragma once
#include <Arduino.h>
#include "Timekeeper.h"

/// @brief Battery voltage reader
class BatteryReader
{
private:
    uint32_t m_vBatPin;
    float m_divRatio;

public:
    /// @brief Initializes BatteryReader class
    /// @param vBatPin Battery sensing pin
    /// @param divRatio Voltage divider ratio. Example: 1M + 1M = ratio of 2.0
    BatteryReader(uint32_t vBatPin, float divRatio) : m_vBatPin(vBatPin), m_divRatio(divRatio) {}

    /// @brief Samples the battery sens pin
    /// @param samples Number of times to read the sens pin
    /// @return The average battery voltage
    float read(uint32_t samples = 10)
    {
        analogRead(m_vBatPin);
        tk.delay(2);

        uint32_t sum_mv = 0;
        for (int i = 0; i < samples; i++)
        {
            sum_mv += analogReadMilliVolts(m_vBatPin);
            tk.delay(2);
        }

        float vadc_mv = sum_mv / (float)samples;
        float vadc_v = vadc_mv / 1000.0f;

        float vbat = vadc_v * m_divRatio;
        return vbat;
    }
};