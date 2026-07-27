#include <Arduino.h>
#include "secrets.h"

#include "WeatherStation.h"

// WiFi Credentials
const char *WIFI_SSID = _WIFI_SSID;
const char *WIFI_PASS = _WIFI_PASS;
const char *SERVER_ENDPOINT = _SERVER_ENDPOINT;

// Number of sensor readings to batch before posting to server
const int BATCH_SIZE = 6;
const int SERIAL_BAUD = 115200;
// DHT22 sensor pin
const int DHT_PIN = 2;

// Battery ADC config
const int VBAT_PIN = 4;
const float DIVIDER_RATIO = 2.0; // 1M + 1M
const int BAT_SAMPLES = 10;

const char *schedule = "* */10 * * * *"; // Every 10 minutes

// Sensors
BME280I2C bmp;
DHT dht(DHT_PIN, DHT22);
BatteryReader bat(VBAT_PIN, DIVIDER_RATIO);

// Persistent data across deep sleep cycles
RTC_DATA_ATTR uint64_t timestamp = 0; // in seconds
RTC_DATA_ATTR uint64_t lastSleepDurationUs = 0;
RTC_DATA_ATTR size_t numReadings = 0;
RTC_DATA_ATTR SensorData sensorReadings[BATCH_SIZE];

WeatherStation station;

void setup()
{
#ifdef STATION_DEBUG
	Serial.begin(SERIAL_BAUD);
	tk.delayMs(3000);

	// while (!Serial)
	// 	;
#endif

	bool success = station.begin(
		WIFI_SSID,
		WIFI_PASS,
		SERVER_ENDPOINT,
		schedule,
		BATCH_SIZE,
		5, // Max WiFi tries
		5, // Max post tries
		&timestamp,
		&lastSleepDurationUs,
		sensorReadings,
		&numReadings,
		&bmp, &dht, &bat);

	while (!success)
	{
		DEBUG_PRINTF("Station setup failed\n");
		station.sleepUntilNextTask();
	}

	DEBUG_PRINTF("Station setup succeeded\n");
}

void loop()
{
	station.loop();
}