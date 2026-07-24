#pragma once

#include <Arduino.h>

/// Timekeeper class that keeps track of time spent in delay
class Timekeeper
{
private:
    uint64_t m_delayUs;

public:
    /// @brief Constructs a timekeeper class with tracked delay = 0
    Timekeeper() : m_delayUs(0) {}

    /// @brief Delays for the specified amount of time and logs it
    /// @param amountMs Amount of time to wait in milliseconds
    void delayMs(uint64_t amountMs)
    {
        m_delayUs += amountMs * 1000;
        ::delay(amountMs);
    }

    /// @brief Delays for the spcified amount of time and logs it
    /// @param amountUs Amount of time to wait in microseconds
    void delayUs(uint64_t amountUs)
    {
        m_delayUs += amountUs;
        ::delayMicroseconds(amountUs);
    }

    /// @brief Time wasted in delay
    /// @return Number of milliseconds
    uint64_t getWastedTime() const
    {
        return m_delayUs / 1000;
    }

    /// @brief Time wasted in delay
    /// @return Number of microseconds
    uint64_t getWastedTimeUs() const
    {
        return m_delayUs;
    }
};

static Timekeeper tk;