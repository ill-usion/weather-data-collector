#pragma once
#include <Arduino.h>
#include <esp_sleep.h>
#include <ccronexpr.h>
#include <ArduinoJson.h>

// WiFi headers
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// Sensor includes
#include <Wire.h>
#include <BME280I2C.h>
#include <DHT.h>

#include "BatteryReader.h"
#include "Timekeeper.h"

// #define STATION_DEBUG

#ifdef STATION_DEBUG
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

// 10 minutes
#define DEEPSLEEP_FALLBACK_DURATION (10 * 60 * 1e6)

typedef struct __attribute__((__packed__))
{
    float temp1, temp2, humidity, pressure, heatIndex, battery;
    uint64_t timestamp;
} SensorData;

class WeatherStation
{
private:
    // Config strings
    String m_ssid, m_pass, m_api;

    // Post settings
    uint32_t m_batchSize, m_maxWiFiTries, m_maxPostTries;

    // Sensors
    BME280I2C *m_bmp;
    DHT *m_dht;
    BatteryReader *m_bat;

    // Persistent data that will be passed in
    uint64_t *m_timestamp, *m_lastSleepDurationUs;
    size_t *m_numReadings;
    SensorData *m_readings;

    // Disposable data
    SensorData m_reading;
    int m_postTryCount;
    bool m_shouldPost;
    bool m_recodedReadings;
    cron_expr m_cronExpr;

public:
    WeatherStation() {}

    /// @brief Constructs a weather station object
    /// @param ssid WiFi ssid
    /// @param pass WiFi password
    /// @param apiEndpoint Server endpoint
    /// @param cronStr Cron expression string
    /// @param batchSize Buffered sensor readings batch size. When number of readings = batchSize, readings are posted.
    /// @param maxWiFiTries How many times to attempt connecting to WiFi
    /// @param maxPostTries How many times to try posting readings before ultimately going to sleep
    /// @param timestamp Current/Estimated timestamp
    /// @param lastSleepDurationUs Last deep sleep duration in microseconds (if station woke up from deepsleep)
    /// @param readings Persistent `RTC_DATA_ATTR` sensor readings
    /// @param numReadings Persistent `RTC_DATA_ATTR` number of sensor readings in `readings`
    /// @param bmp BMP/BME sensor instance
    /// @param dht DHT22 sensor instance
    /// @param bat Battery reader instance
    /// @return True if setup was successful
    bool begin(
        const char *ssid,
        const char *pass,
        const char *apiEndpoint,
        const char *cronStr,
        uint32_t batchSize,
        uint32_t maxWiFiTries,
        uint32_t maxPostTries,
        uint64_t *timestamp,
        uint64_t *lastSleepDurationUs,
        SensorData *readings,
        size_t *numReadings,
        BME280I2C *bmp,
        DHT *dht,
        BatteryReader *bat);

    /// @brief Executes the station routine
    void loop();

    /// @brief Calculates the time left until next task specified by cron expression and sleeps for that amount
    void sleepUntilNextTask();

private:
    /// @brief Initializes the sensors (BME280 and DHT22)
    void initSensors();

    /// @brief Reads weather station sensors
    /// @return Sensor data
    SensorData readSensors();

    /// @brief Updates the timestamp either by estimating or requesting it from the server
    /// @param syncFromServer Whether to force sync time from the server instead of estimating it
    /// @return Updated timestamp if successful. 0 if requesting timestamp from server failed, 1 if no WiFi
    uint64_t getUpdatedTimestamp(bool syncFromServer = false);

    /// @brief Requests the current timestamp from the server
    /// @return Timestamp if successful, 0 if request failed
    uint64_t requestTimestamp();

    /// @brief Connects to WiFi using the given ssid and pass present in the member vars
    /// @return True if connected to WiFi
    bool connectToWiFi();

    /// @brief Posts the readings present in member variables
    /// @return If posting succeeded
    bool postBatch();

    /// @brief Puts the station into deep sleep mode and sets up a wake up timer
    /// @param amountUs Amount of time to deep sleep for in microseconds
    void goToSleep(uint64_t amountUs);
};
