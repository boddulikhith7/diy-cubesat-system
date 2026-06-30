/*
 * config.example.h - Configuration template for the DIY CubeSat/CanSat Flight Computer.
 * Copy this file to "config.h" and edit it with your desired configuration.
 */

#ifndef CONFIG_H
#define CONFIG_H

// --- Wi-Fi Configuration ---
#define AP_SSID "Satellite-Brain-01"
#define AP_PASS "12345678"          // Minimum 8 characters
#define AP_CHANNEL 1                // Wi-Fi channel
#define AP_HIDDEN 0                 // 0 = visible, 1 = hidden

// --- Web Server Port ---
#define WEBSERVER_PORT 80

// --- MicroSD SPI Chip Select Pin ---
#define SD_CS_PIN 8                 // NodeMCU pin D8 (GPIO15)

// --- MPU6050 and BMP280 Pins ---
// MPU6050 and BMP280 communicate over I2C.
// NodeMCU default I2C pins:
// SDA = D2 (GPIO4)
// SCL = D1 (GPIO5)

// --- Telemetry Logging Settings ---
#define LOG_INTERVAL_MS 1000        // Log data to SD card every 1 second
#define TELEMETRY_FILE_NAME "datalog.csv"

#endif // CONFIG_H
