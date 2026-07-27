#include "WeatherStation.h"

bool WeatherStation::begin(
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
    BatteryReader *bat)
{
    m_ssid = ssid;
    m_pass = pass;
    m_api = apiEndpoint;
    m_batchSize = batchSize;
    m_maxWiFiTries = maxWiFiTries;
    m_maxPostTries = maxPostTries;
    m_timestamp = timestamp;
    m_lastSleepDurationUs = lastSleepDurationUs;
    m_readings = readings;
    m_numReadings = numReadings;

    m_bmp = bmp;
    m_dht = dht;
    m_bat = bat;

    uint32_t fsMountTries = 0;
    const uint32_t MAX_FS_MOUNT_TRIES = 5;
    while (!LittleFS.begin(true) && fsMountTries < MAX_FS_MOUNT_TRIES)
    {
        tk.delayMs(100);
        fsMountTries++;
    }

    if (fsMountTries == MAX_FS_MOUNT_TRIES)
    {
        DEBUG_PRINTF("Failed to mount LittleFS\n");
        return false;
    }

    DEBUG_PRINTF("LittleFS mounted successfully\n");

    m_dataStore = DataStore::open(DATASTORE_FILENAME);
    DEBUG_PRINTF("Data store size: %d bytes\n", m_dataStore.size());

    if (m_timestamp == NULL)
    {
        DEBUG_PRINTF("`timestamp` is null\n");
        return false;
    }

    if (m_lastSleepDurationUs == NULL)
    {
        (*m_lastSleepDurationUs) = 0;
    }

    if (m_readings == NULL)
    {
        DEBUG_PRINTF("`readings` is null\n");
        return false;
    }

    if (m_numReadings == NULL)
    {
        DEBUG_PRINTF("`numReadings` is null\n");
        return false;
    }

    if (m_dht == NULL)
    {
        DEBUG_PRINTF("`dht` is null\n");
        return false;
    }

    if (m_bmp == NULL)
    {
        DEBUG_PRINTF("`bmp` is null\n");
        return false;
    }

    if (m_bat == NULL)
    {
        DEBUG_PRINTF("`bat` is null\n");
        return false;
    }

    const char *err;
    cron_parse_expr(cronStr, &m_cronExpr, &err);
    if (err != NULL)
    {
        DEBUG_PRINTF("Error parsing cron expression: %s\n", cronStr);
        return false;
    }

    m_postTryCount = 0;
    m_shouldPost = (*m_numReadings) >= m_batchSize;
    m_recodedReadings = false;
    m_addedBatchToFile = false;

    initSensors();

    uint64_t _timestamp;
    esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
    // If we did not wake up after deep sleep or timestamp is 0
    if (*m_timestamp == 0 || wakeupCause != ESP_SLEEP_WAKEUP_TIMER)
    {
        bool connected = connectToWiFi();
        if (!connected)
        {
            DEBUG_PRINTF("Failed to connect to WiFi. Cannot request timestamp.\n");
            return false;
        }

        _timestamp = getUpdatedTimestamp(true);
        if (_timestamp == 0 || _timestamp == 1)
        {
            DEBUG_PRINTF("Failed to update timestamp. Error code %llu\n", _timestamp);
            return false;
        }

        *m_timestamp = _timestamp;
        DEBUG_PRINTF("Requested and updated timestamp successfully. Current timestamp: %llu\n", *m_timestamp);
    }
    else
    {
        // Logically we should not require an internet
        // connection since timestamp will be estimated
        _timestamp = getUpdatedTimestamp();
        if (_timestamp == 0 || _timestamp == 1)
        {
            DEBUG_PRINTF("Code should not reach here.\n");
            return false;
        }

        *m_timestamp = _timestamp;
    }

    return true;
}

void WeatherStation::loop()
{
    // Check if we exceeded the trial limit
    if (m_postTryCount >= m_maxPostTries)
    {
        if (!m_addedBatchToFile)
        {
            size_t s = m_dataStore.store((uint8_t *)m_readings, sizeof(SensorData) * (*m_numReadings));
            *m_numReadings = 0;
            DEBUG_PRINTF("Logged %d bytes of readings to data store. ", s);
        }

        DEBUG_PRINTF("Exceeded max post tries. Going to sleep...\n", s);
        sleepUntilNextTask();
    }

    // Check if we should post data
    if (m_shouldPost)
    {
        m_postTryCount++;
        DEBUG_PRINTF("Attempt %d: ", m_postTryCount);

        // Attempt to connect to WiFi
        if (WiFi.status() != WL_CONNECTED && !connectToWiFi())
        {
            DEBUG_PRINTF("Could not connect to WiFi.\n");
            return;
        }

        // Check if there is any buffered data and attempt to post it
        if (m_dataStore.size() > 0)
        {
            // Add the current batch to data store
            if (!m_addedBatchToFile)
            {
                m_dataStore.store((uint8_t *)m_readings, sizeof(SensorData) * (*m_numReadings));
                *m_numReadings = 0;
                m_addedBatchToFile = true;
            }

            // Try and post the data
            File f = m_dataStore.getReader();
            bool success = postReadingsFile(&f);
            f.close();
            if (!success)
            {
                DEBUG_PRINTF("Failed to post readings file.\n");
                return;
            }

            DEBUG_PRINTF("Successfully posted readings file of size %d bytes.\n", f.size());
            // Clear data store after successful post
            m_dataStore.clear();
        }
        // Otherwise post the current batch
        else
        {
            bool success = postBatch();
            if (!success)
            {
                DEBUG_PRINTF("Failed to post batch.\n");
                return;
            }

            DEBUG_PRINTF("Successfully posted batch of %d readings.\n", *m_numReadings);
            // Clear batch after successful post
            *m_numReadings = 0;
        }

        // Everything succeeded
        // Update timestamp fromserver
        uint64_t _timestamp = getUpdatedTimestamp(true);
        if (_timestamp != 0 || _timestamp != 1)
            *m_timestamp = _timestamp;

        // Sleep until next routine
        sleepUntilNextTask();
    }
    // Have not recorded readings this routine
    else if (!m_recodedReadings)
    {
        m_reading = readSensors();
        DEBUG_PRINTF("Reading sensors...\n");
        m_readings[(*m_numReadings)++] = m_reading;
        m_recodedReadings = true;
        m_shouldPost = (*m_numReadings) >= m_batchSize;
    }
    // None of the above tasks are required
    else
    {
        DEBUG_PRINTF("Nothing to do. Going to sleep...\n");
        sleepUntilNextTask();
    }
}

void WeatherStation::initSensors()
{
    Wire.begin();
    while (!m_bmp->begin())
        tk.delayMs(10);

    m_dht->begin();
}

SensorData WeatherStation::readSensors()
{
    float t1, t2, p, h, hi, bat;

    m_bmp->read(p, t1, h);

    h = m_dht->readHumidity();
    t2 = m_dht->readTemperature();
    hi = m_dht->computeHeatIndex(t1, h, false);
    bat = m_bat->read();

    return SensorData{
        .temp1 = t1,
        .temp2 = t2,
        .humidity = h,
        .pressure = p,
        .heatIndex = hi,
        .battery = bat,
        .timestamp = *m_timestamp};
}

uint64_t WeatherStation::getUpdatedTimestamp(bool syncFromServer)
{
    esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();

    // Guard clause
    if (!syncFromServer && wakeupCause == ESP_SLEEP_WAKEUP_TIMER)
    {
        auto us2s = [](uint64_t us)
        { return us / 1e6; };
        uint64_t newTimestamp = (*m_timestamp);
        newTimestamp += us2s(*m_lastSleepDurationUs);
        newTimestamp += us2s(tk.getWastedTimeUs());

        return newTimestamp;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        DEBUG_PRINTF("Cannot request timestamp from server. No internet connection.\n");
        return 1;
    }

    return requestTimestamp();
}

uint64_t WeatherStation::requestTimestamp()
{
    HTTPClient client;
    client.begin(m_api + "/timestamp");
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

bool WeatherStation::connectToWiFi()
{
    uint32_t tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < m_maxWiFiTries)
    {
        WiFi.begin(m_ssid, m_pass);
        DEBUG_PRINTF("Attempt %d: Connecting to WiFi...\n", tries + 1);
        tk.delayMs(1000);
        tries++;
    }

    return WiFi.status() == WL_CONNECTED;
}

bool WeatherStation::postBatch()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        DEBUG_PRINTF("Failed to post batch. No internet connection.\n");
        return false;
    }

    JsonDocument doc;
    doc["entries"] = JsonArray();
    for (size_t i = 0; i < *m_numReadings; i++)
    {
        JsonObject entry = doc["entries"].add<JsonObject>();
        entry["timestamp"] = m_readings[i].timestamp;
        entry["temp1"] = m_readings[i].temp1;
        entry["temp2"] = m_readings[i].temp2;
        entry["humidity"] = m_readings[i].humidity;
        entry["pressure"] = m_readings[i].pressure;
        entry["heat_index"] = m_readings[i].heatIndex;
        entry["battery"] = m_readings[i].battery;
    }

    HTTPClient client;
    client.begin(m_api + "/batch-submit");
    int code = client.POST(doc.as<String>());

    return code == 204;
}

bool WeatherStation::postReadingsFile(File *file)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        DEBUG_PRINTF("Failed to post readings file. No internet connection.\n");
        return false;
    }

    HTTPClient client;
    client.begin(m_api + "/binary-submit");
    int code = client.sendRequest("POST", file, file->size());

    return code == 204;
}

void WeatherStation::goToSleep(uint64_t amountUs)
{
    *m_lastSleepDurationUs = amountUs;

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    btStop();

    gpio_hold_en(GPIO_NUM_18);
    gpio_hold_en(GPIO_NUM_19);
    gpio_deep_sleep_hold_en();

    esp_sleep_enable_timer_wakeup(amountUs);
    esp_deep_sleep_start();
}

void WeatherStation::sleepUntilNextTask()
{
    if (m_timestamp == 0  || m_timestamp == NULL)
    {
        DEBUG_PRINTF("Cannot schedule next task. Sleeping for fallback duration.\n");
        goToSleep(DEEPSLEEP_FALLBACK_DURATION - tk.getWastedTimeUs());
    }

    uint64_t timestamp = *m_timestamp;
    DEBUG_PRINTF("Current timestamp=%llu\n", timestamp);
    // Roll to the next minute to avoid cron library rescheduling the same timestamp
    time_t now = (int64_t)timestamp + 60;
    time_t next = cron_next(&m_cronExpr, now);
    if (next == ((time_t)-1))
    {
        DEBUG_PRINTF("Error calculating next cron time\n");
        goToSleep(DEEPSLEEP_FALLBACK_DURATION - tk.getWastedTimeUs());
    }
    // now and next are overflowing uint64_t values, so we need to cast them to int64_t
    DEBUG_PRINTF("Scheduled sleep: Current timestamp=%lld, Next cron timestamp=%lld\n", (int64_t)now, (int64_t)next);
    uint64_t sleepDurationUs = ((uint64_t)(next - now + 60)) * 1e6; // + 60 to compensate for the rolling to the next minute above
    uint64_t offsetUs = tk.getWastedTimeUs();
    if (sleepDurationUs > offsetUs)
        sleepDurationUs -= offsetUs; // Subtract time wasted in delay

    DEBUG_PRINTF("Going to sleep for %llu microseconds\n", sleepDurationUs);
    goToSleep(sleepDurationUs);
}