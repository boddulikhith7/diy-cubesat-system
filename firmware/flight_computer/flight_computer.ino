/*
 * flight_computer.ino - DIY CanSat/CubeSat ESP8266 Flight Computer Firmware
 * Features: Local AP, Web Telemetry Server, MPU6050, BMP280, SD Logger
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>

#include "config.h"

// Instance definitions
ESP8266WebServer server(WEBSERVER_PORT);
Adafruit_BMP280 bmp;
Adafruit_MPU6050 mpu;

// Global sensor readings
float temperature = 0.0;
float pressure = 0.0;
float altitude = 0.0;
float accelX = 0.0, accelY = 0.0, accelZ = 0.0;
float gyroX = 0.0, gyroY = 0.0, gyroZ = 0.0;

bool bmpReady = false;
bool mpuReady = false;
bool sdReady = false;

// Timing variables
unsigned long lastLogTime = 0;
unsigned long logCount = 0;

// Offline Dashboard HTML page
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Satellite-Brain-01 Mission Control</title>
    <style>
        :root {
            --bg-color: #0d1117;
            --panel-bg: rgba(22, 27, 34, 0.85);
            --border-color: #30363d;
            --accent-green: #2ea043;
            --accent-blue: #58a6ff;
            --accent-orange: #f0883e;
            --text-color: #c9d1d9;
            --text-dim: #8b949e;
        }

        body {
            background-color: var(--bg-color);
            color: var(--text-color);
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
            margin: 0;
            padding: 20px;
        }

        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 2px solid var(--border-color);
            padding-bottom: 15px;
            margin-bottom: 20px;
        }

        h1 {
            font-size: 24px;
            margin: 0;
            background: linear-gradient(45deg, var(--accent-blue), #bc8cff);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        .status-bar {
            display: flex;
            gap: 15px;
        }

        .badge {
            font-size: 11px;
            font-weight: 600;
            padding: 4px 8px;
            border-radius: 12px;
            border: 1px solid var(--border-color);
            background: var(--panel-bg);
        }

        .badge.online { border-color: var(--accent-green); color: var(--accent-green); }
        .badge.offline { border-color: #da3637; color: #da3637; }

        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }

        .panel {
            background: var(--panel-bg);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
            backdrop-filter: blur(5px);
        }

        .panel h2 {
            font-size: 14px;
            color: var(--text-dim);
            margin-top: 0;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 8px;
        }

        .telemetry-val {
            font-size: 32px;
            font-weight: 700;
            margin: 15px 0 5px 0;
            color: #ffffff;
        }

        .unit {
            font-size: 16px;
            font-weight: 400;
            color: var(--text-dim);
            margin-left: 4px;
        }

        .sensor-sub {
            font-size: 12px;
            color: var(--text-dim);
            display: flex;
            justify-content: space-between;
        }

        .graph-container {
            width: 100%;
            height: 120px;
            margin-top: 15px;
            border-top: 1px solid var(--border-color);
            padding-top: 10px;
        }

        canvas {
            width: 100%;
            height: 100%;
        }

        .artificial-horizon {
            display: flex;
            justify-content: center;
            align-items: center;
            height: 150px;
            margin: 15px 0;
        }

        .horizon-disc {
            width: 120px;
            height: 120px;
            border-radius: 50%;
            border: 3px solid var(--border-color);
            position: relative;
            overflow: hidden;
            background: linear-gradient(to bottom, #58a6ff 50%, #8b5a2b 50%);
            transition: transform 0.1s ease-out;
        }

        .horizon-line {
            width: 100%;
            height: 2px;
            background: #ffffff;
            position: absolute;
            top: 50%;
        }

        .horizon-indicator {
            width: 10px;
            height: 10px;
            background: var(--accent-orange);
            border-radius: 50%;
            position: absolute;
            top: calc(50% - 5px);
            left: calc(50% - 5px);
            z-index: 10;
        }
    </style>
</head>
<body>
    <header>
        <div>
            <h1>SATELLITE-BRAIN-01</h1>
            <div style="font-size: 12px; color: var(--text-dim)">Local Mission Control System</div>
        </div>
        <div class="status-bar">
            <span id="stat-bmp" class="badge offline">BMP280</span>
            <span id="stat-mpu" class="badge offline">MPU6050</span>
            <span id="stat-sd" class="badge offline">SD CARD</span>
        </div>
    </header>

    <div class="dashboard-grid">
        <!-- Altimeter Panel -->
        <div class="panel">
            <h2>Altitude (BMP280)</h2>
            <div class="telemetry-val"><span id="val-alt">0.00</span><span class="unit">m</span></div>
            <div class="sensor-sub">
                <span>Pressure: <span id="val-press">0.0</span> hPa</span>
                <span>Temp: <span id="val-temp">0.0</span> °C</span>
            </div>
            <div class="graph-container">
                <canvas id="alt-chart"></canvas>
            </div>
        </div>

        <!-- Attitude IMU Panel -->
        <div class="panel">
            <h2>Attitude Horizon (MPU6050)</h2>
            <div class="artificial-horizon">
                <div id="horizon-disc" class="horizon-disc">
                    <div class="horizon-line"></div>
                </div>
                <div class="horizon-indicator"></div>
            </div>
            <div class="sensor-sub" style="justify-content: space-around;">
                <span>Pitch: <span id="val-pitch">0</span>°</span>
                <span>Roll: <span id="val-roll">0</span>°</span>
            </div>
        </div>

        <!-- Acceleration G-Force Panel -->
        <div class="panel">
            <h2>Acceleration forces</h2>
            <div class="telemetry-val"><span id="val-g">1.00</span><span class="unit">G</span></div>
            <div class="sensor-sub">
                <span>X: <span id="val-ax">0.00</span> m/s²</span>
                <span>Y: <span id="val-ay">0.00</span> m/s²</span>
                <span>Z: <span id="val-az">9.81</span> m/s²</span>
            </div>
            <div class="graph-container">
                <canvas id="g-chart"></canvas>
            </div>
        </div>
    </div>

    <div class="panel">
        <h2>Flight Record status</h2>
        <div class="sensor-sub" style="font-size: 14px;">
            <span>Logs Written: <span id="val-logs" style="font-weight:bold; color:#fff">0</span></span>
            <span>Local Uptime: <span id="val-uptime" style="font-weight:bold; color:#fff">00:00:00</span></span>
        </div>
    </div>

    <script>
        let altData = [];
        let gData = [];
        const maxDataPoints = 30;

        function updateBadge(id, status) {
            const badge = document.getElementById(id);
            if (status) {
                badge.className = 'badge online';
            } else {
                badge.className = 'badge offline';
            }
        }

        function drawChart(canvasId, dataPoints, color) {
            const canvas = document.getElementById(canvasId);
            const ctx = canvas.getContext('2d');
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            if (dataPoints.length < 2) return;

            const width = canvas.width;
            const height = canvas.height;
            const minVal = Math.min(...dataPoints);
            const maxVal = Math.max(...dataPoints);
            const range = maxVal - minVal === 0 ? 1 : maxVal - minVal;

            ctx.strokeStyle = color;
            ctx.lineWidth = 2;
            ctx.beginPath();

            for (let i = 0; i < dataPoints.length; i++) {
                const x = (i / (dataPoints.length - 1)) * width;
                const y = height - ((dataPoints[i] - minVal) / range) * (height - 10) - 5;
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.stroke();
        }

        function fetchTelemetry() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    // Update connection badges
                    updateBadge('stat-bmp', data.sensors.bmp);
                    updateBadge('stat-mpu', data.sensors.mpu);
                    updateBadge('stat-sd', data.sensors.sd);

                    // Update numeric values
                    document.getElementById('val-alt').innerText = data.telemetry.altitude.toFixed(2);
                    document.getElementById('val-press').innerText = data.telemetry.pressure.toFixed(1);
                    document.getElementById('val-temp').innerText = data.telemetry.temp.toFixed(1);
                    document.getElementById('val-ax').innerText = data.telemetry.ax.toFixed(2);
                    document.getElementById('val-ay').innerText = data.telemetry.ay.toFixed(2);
                    document.getElementById('val-az').innerText = data.telemetry.az.toFixed(2);
                    
                    // Calc total G force
                    const ax = data.telemetry.ax;
                    const ay = data.telemetry.ay;
                    const az = data.telemetry.az;
                    const gForce = Math.sqrt(ax*ax + ay*ay + az*az) / 9.80665;
                    document.getElementById('val-g').innerText = gForce.toFixed(2);

                    // Update status counts
                    document.getElementById('val-logs').innerText = data.status.logs;
                    document.getElementById('val-uptime').innerText = data.status.uptime;

                    // Update Attitude Pitch & Roll (artificial horizon)
                    // Simple estimation from accelerometer
                    let roll = Math.atan2(ay, az) * 57.29578;
                    let pitch = Math.atan2(-ax, Math.sqrt(ay*ay + az*az)) * 57.29578;
                    
                    document.getElementById('val-roll').innerText = Math.round(roll);
                    document.getElementById('val-pitch').innerText = Math.round(pitch);

                    // Rotate the artificial horizon disc
                    const disc = document.getElementById('horizon-disc');
                    disc.style.transform = `rotate(${roll}deg) translateY(${pitch * 0.8}px)`;

                    // Append chart data
                    altData.push(data.telemetry.altitude);
                    if (altData.length > maxDataPoints) altData.shift();
                    drawChart('alt-chart', altData, '#58a6ff');

                    gData.push(gForce);
                    if (gData.length > maxDataPoints) gData.shift();
                    drawChart('g-chart', gData, '#2ea043');
                })
                .catch(err => console.error("Telemetry fetch failed: ", err));
        }

        // Adjust canvas resolution
        document.querySelectorAll('canvas').forEach(canvas => {
            canvas.width = canvas.clientWidth;
            canvas.height = canvas.clientHeight;
        });

        setInterval(fetchTelemetry, 250);
    </script>
</body>
</html>
)rawliteral";

// Helper function to format uptime (ms to HH:MM:SS)
String getFormattedUptime() {
    unsigned long sec = millis() / 1000;
    unsigned long hh = sec / 3600;
    unsigned long mm = (sec % 3600) / 60;
    unsigned long ss = sec % 60;
    
    char buffer[12];
    sprintf(buffer, "%02lu:%02lu:%02lu", hh, mm, ss);
    return String(buffer);
}

void handleRoot() {
    server.send(200, "text/html", INDEX_HTML);
}

void handleData() {
    String json = "{";
    
    // Connection Status
    json += "\"sensors\":{";
    json += "\"bmp\":" + String(bmpReady ? "true" : "false") + ",";
    json += "\"mpu\":" + String(mpuReady ? "true" : "false") + ",";
    json += "\"sd\":" + String(sdReady ? "true" : "false");
    json += "},";

    // Live Telemetry Readings
    json += "\"telemetry\":{";
    json += "\"temp\":" + String(temperature) + ",";
    json += "\"pressure\":" + String(pressure) + ",";
    json += "\"altitude\":" + String(altitude) + ",";
    json += "\"ax\":" + String(accelX) + ",";
    json += "\"ay\":" + String(accelY) + ",";
    json += "\"az\":" + String(accelZ) + ",";
    json += "\"gx\":" + String(gyroX) + ",";
    json += "\"gy\":" + String(gyroY) + ",";
    json += "\"gz\":" + String(gyroZ);
    json += "},";

    // System Status
    json += "\"status\":{";
    json += "\"logs\":" + String(logCount) + ",";
    json += "\"uptime\":\"" + getFormattedUptime() + "\"";
    json += "}";
    
    json += "}";
    server.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n--- Satellite-Brain-01 System Initializing ---");

    // Initialize I2C
    Wire.begin(4, 5); // SDA = GPIO4 (D2), SCL = GPIO5 (D1)

    // 1. Initialize BMP280
    if (bmp.begin(0x76)) { // 0x76 is default address for BMP280
        Serial.println("BMP280 Sensor: OK");
        bmpReady = true;
        
        // Default altimeter settings
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X2,
                        Adafruit_BMP280::SAMPLING_X16,
                        Adafruit_BMP280::FILTER_X16,
                        Adafruit_BMP280::STANDBY_MS_500);
    } else {
        Serial.println("BMP280 Sensor: FAILED");
    }

    // 2. Initialize MPU6050
    if (mpu.begin()) {
        Serial.println("MPU6050 Sensor: OK");
        mpuReady = true;
        
        mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    } else {
        Serial.println("MPU6050 Sensor: FAILED");
    }

    // 3. Initialize SD Card
    if (SD.begin(SD_CS_PIN)) {
        Serial.println("SD Card Module: OK");
        sdReady = true;
        
        // Write CSV header if file doesn't exist
        if (!SD.exists(TELEMETRY_FILE_NAME)) {
            File logFile = SD.open(TELEMETRY_FILE_NAME, FILE_WRITE);
            if (logFile) {
                logFile.println("Uptime_Sec,Temp_C,Pressure_hPa,Alt_m,AccelX,AccelY,AccelZ,GyroX,GyroY,GyroZ");
                logFile.close();
            }
        }
    } else {
        Serial.println("SD Card Module: FAILED");
    }

    // 4. Configure local Access Point (AP) Mode
    WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, AP_HIDDEN);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("Local Web Dashboard Gateway IP: ");
    Serial.println(IP);

    // 5. Setup Web Server Endpoints
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.begin();
    Serial.println("HTTP server started.");
}

void loop() {
    server.handleClient();

    // Read sensors
    if (bmpReady) {
        temperature = bmp.readTemperature();
        pressure = bmp.readPressure() / 100.0F; // Pa to hPa
        altitude = bmp.readAltitude(1013.25);  // sea level hPa calibration
    }
    
    if (mpuReady) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        accelX = a.acceleration.x;
        accelY = a.acceleration.y;
        accelZ = a.acceleration.z;
        gyroX = g.gyro.x;
        gyroY = g.gyro.y;
        gyroZ = g.gyro.z;
    }

    // Log to SD card based on config interval
    unsigned long currentMillis = millis();
    if (sdReady && (currentMillis - lastLogTime >= LOG_INTERVAL_MS)) {
        lastLogTime = currentMillis;
        
        File logFile = SD.open(TELEMETRY_FILE_NAME, FILE_WRITE);
        if (logFile) {
            logFile.print(currentMillis / 1000.0); logFile.print(",");
            logFile.print(temperature); logFile.print(",");
            logFile.print(pressure); logFile.print(",");
            logFile.print(altitude); logFile.print(",");
            logFile.print(accelX); logFile.print(",");
            logFile.print(accelY); logFile.print(",");
            logFile.print(accelZ); logFile.print(",");
            logFile.print(gyroX); logFile.print(",");
            logFile.print(gyroY); logFile.print(",");
            logFile.println(gyroZ);
            logFile.close();
            logCount++;
        }
    }
}
