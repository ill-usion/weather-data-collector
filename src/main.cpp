#include <Arduino.h>
#include <esp_sleep.h>

#include <Wire.h>
#include <BME280I2C.h>
#include <DHT.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <ArduinoJson.h>
#include "secrets.h"

/* Parameters */

// WiFi Credentials
const char *WIFI_SSID = _WIFI_SSID;
const char *WIFI_PASS = _WIFI_PASS;
const String SERVER_ENDPOINT = _SERVER_ENDPOINT;

// Number of sensor readings to batch before posting to server
const int BATCH_SIZE = 6;
// Number of times to attempt to post data before going to deep sleep
const int MAX_POST_TRIES = 5;
const uint64_t S_TO_US = 1000000;
// Deep sleep duration in microseconds
const uint64_t DEEPSLEEP_DURATION = (10 * 60 * S_TO_US);
const int SERIAL_BAUD = 115200;
// DHT22 sensor pin
const int DHT_PIN = 2;

// Battery ADC config
const int VBAT_PIN = 4;
const float DIVIDER_RATIO = 2.0; // 1M + 1M
const int BAT_SAMPLES = 10;

typedef struct
{
	float temp1, temp2, humidity, pressure, heatIndex, battery;
	uint64_t timestamp;
} SensorData;

// Sensors
BME280I2C bmp;
DHT dht(DHT_PIN, DHT22);

// Persistent data across deep sleep cycles
RTC_DATA_ATTR uint64_t timestamp;
RTC_DATA_ATTR uint64_t lastSleepDuration;
RTC_DATA_ATTR size_t numReadings = 0;
RTC_DATA_ATTR SensorData sensorReadings[BATCH_SIZE];

// Disposable data for current routine
SensorData data;
int tryCount;
uint64_t delayAmountMs;
bool shouldPost = false;

void trackedDelay(uint64_t amount);
bool connectToWiFi(uint32_t maxRetries = 10);
void initSensors();
SensorData readSensors();
void dryRunSensors(uint32_t n = 10, uint32_t intervalMs = 10);
void goToSleep(uint64_t amount);
float readBatteryVoltage();
uint64_t requestTimestamp();
bool updateTimestamp(bool syncFromServer = false);
bool postBatch(SensorData *data, size_t n);

void setup()
{
	tryCount = 0;
	delayAmountMs = 0;
	shouldPost = numReadings >= BATCH_SIZE;

	Serial.begin(SERIAL_BAUD);
	while (!Serial)
		;

	// WiFi will be connected to on demand later on inside function calls
	initSensors();
	trackedDelay(500);

	if (!updateTimestamp())
	{
		goToSleep(DEEPSLEEP_DURATION - delayAmountMs * 1000);
	}

	// Should not post = get more readings
	if (!shouldPost)
	{
		// Sample sensors a few times to get a more accurate reading
		dryRunSensors();

		data = readSensors();
		sensorReadings[numReadings++] = data;
		shouldPost = numReadings >= BATCH_SIZE;
	}
}

void loop()
{
	// Serial.printf("Temperature1: %.2fC, Temperature2: %.2f, Humidity: %.2f%%, Pressure: %.2fhPa, HI: %.2fC, Battery: %.2fV\n", data.temp1, data.temp2, data.humidity, data.pressure, data.heatIndex, data.battery);
	if (shouldPost)
	{
		bool success = postBatch(sensorReadings, numReadings);
		if (success || tryCount >= MAX_POST_TRIES)
		{
			updateTimestamp(true);
			numReadings = 0;
			goToSleep(DEEPSLEEP_DURATION - delayAmountMs * 1000);
		}
		else
		{
			tryCount++;
		}
	}
	else
	{
		goToSleep(DEEPSLEEP_DURATION - delayAmountMs * 1000);
	}

	trackedDelay(1000);
}

void trackedDelay(uint64_t amount)
{
	delay(amount);
	delayAmountMs += amount;
}

bool connectToWiFi(uint32_t maxRetries)
{
	uint32_t tries = 0;

	while (WiFi.status() != WL_CONNECTED && tries < maxRetries)
	{
		WiFi.begin(WIFI_SSID, WIFI_PASS);
		Serial.printf("Attempt %d: Connecting to Wi-Fi...", tries + 1);
		trackedDelay(3000);
		tries++;
	}

	return WiFi.status() == WL_CONNECTED;
}

void initSensors()
{
	Wire.begin();
	while (!bmp.begin())
		trackedDelay(50);
	dht.begin();
}

SensorData readSensors()
{
	float t1, t2, p, h, hi, bat;

	bmp.read(p, t1, h);

	h = dht.readHumidity();
	t2 = dht.readTemperature();
	hi = dht.computeHeatIndex(t1, h, false);
	bat = readBatteryVoltage();

	return SensorData{
		.temp1 = t1,
		.temp2 = t2,
		.humidity = h,
		.pressure = p,
		.heatIndex = hi,
		.battery = bat,
		.timestamp = timestamp};
}

void dryRunSensors(uint32_t n, uint32_t intervalMs)
{
	for (uint32_t i = 0; i < n; i++)
	{
		readSensors();
		trackedDelay(intervalMs);
	}
}

void goToSleep(uint64_t amount)
{
	lastSleepDuration = amount;

	pinMode(8, OUTPUT);
	digitalWrite(8, HIGH);
	delay(500);
	digitalWrite(8, LOW);

	WiFi.disconnect(true);
	WiFi.mode(WIFI_OFF);

	btStop();

	gpio_hold_en(GPIO_NUM_18);
	gpio_hold_en(GPIO_NUM_19);
	gpio_deep_sleep_hold_en();

	esp_sleep_enable_timer_wakeup(amount);
	esp_deep_sleep_start();
}

float readBatteryVoltage()
{
	analogRead(VBAT_PIN);
	trackedDelay(2);

	uint32_t sum_mv = 0;
	for (int i = 0; i < BAT_SAMPLES; i++)
	{
		sum_mv += analogReadMilliVolts(VBAT_PIN);
		trackedDelay(2);
	}

	float vadc_mv = sum_mv / (float)BAT_SAMPLES;
	float vadc_v = vadc_mv / 1000.0f;

	float vbat = vadc_v * DIVIDER_RATIO;
	return vbat;
}

uint64_t requestTimestamp()
{
	HTTPClient client;
	client.begin(SERVER_ENDPOINT + "/timestamp");
	int status = client.GET();
	if (status != 200)
		return 0;

	String tStr = client.getString();
	for (int i = 0; i < tStr.length(); i++)
	{
		if (isdigit(tStr.charAt(i) == 0))
			return 0;
	}

	return tStr.toInt();
}

bool updateTimestamp(bool syncFromServer)
{
	if (!syncFromServer && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER)
	{
		// Woke up from deepsleep
		timestamp += (lastSleepDuration * ( 1.0 / S_TO_US) + (delayAmountMs / 1000)); // + time wasted
		return true;
	}
	else
	{
		// Make sure we are connected to WiFi before requesting timestamp
		if (WiFi.status() != WL_CONNECTED)
		{
			bool connected = connectToWiFi();
			if (!connected)
				return false;
		}

		// Woke up from reset or forced to sync from server
		uint64_t _timestamp = requestTimestamp();
		if (_timestamp != 0)
		{
			// Update time if we have a valid timestamp
			timestamp = _timestamp;
		}
		return _timestamp != 0;
	}

	__builtin_unreachable();
}

bool postBatch(SensorData *data, size_t n)
{
	// Connect to WiFi on demand
	if (WiFi.status() != WL_CONNECTED)
	{
		bool connected = connectToWiFi();
		if (!connected)
			return false;
	}

	JsonDocument doc;
	doc["entries"] = JsonArray();
	for (size_t i = 0; i < n; i++)
	{
		JsonObject entry = doc["entries"].add<JsonObject>();
		entry["timestamp"] = data[i].timestamp;
		entry["temp1"] = data[i].temp1;
		entry["temp2"] = data[i].temp2;
		entry["humidity"] = data[i].humidity;
		entry["pressure"] = data[i].pressure;
		entry["heat_index"] = data[i].heatIndex;
		entry["battery"] = data[i].battery;
	}

	HTTPClient client;
	client.begin(SERVER_ENDPOINT + "/batch-submit");
	int code = client.POST(doc.as<String>());

	return code == 204;
}