#pragma once

// microSD card (SDIO 4-bit) and the sensor CSV log on it
bool beginSdCard();
bool sdCardReady();
void logSensorData();       // append one row to /sensor_log.csv now
void logSensorDataIfDue();  // call from loop(); logs every 5 minutes
