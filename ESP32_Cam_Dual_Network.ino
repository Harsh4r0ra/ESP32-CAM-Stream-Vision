/*
 * ESP32-CAM Dual Network
 * 
 * This firmware enables the ESP32-CAM to connect to a WiFi network while
 * maintaining its own Access Point as a fallback. It provides camera streaming,
 * sensor monitoring, and a web interface.
 * 
 * Part of the ESP32-CAM Stream Vision project
 * https://github.com/yourusername/ESP32-CAM-Stream-Vision
 */

#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"

// Camera pins for AI Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// WiFi client mode settings (your hotspot credentials)
const char* wifi_ssid = "YOUR_HOTSPOT_SSID";      // CHANGE THIS
const char* wifi_password = "YOUR_HOTSPOT_PASSWORD"; // CHANGE THIS

// Access Point settings (keeping the existing AP mode functionality)
const char* ap_ssid = "ESP32-CAM-AP";
const char* ap_password = "12345678";

// Maximum connection attempts before falling back to AP mode only
const int MAX_CONNECTION_ATTEMPTS = 20;

// Web server port
WebServer server(80);

// Data structure for storing sensor data
struct SensorData {
  float temperature;
  float humidity;
  int waterSensor;
};

// Sensor data variable
SensorData sensorData;

// Timestamp of last data update
unsigned long lastDataReceived = 0;

// Flag to indicate if camera was initialized successfully
bool cameraInitialized = false;

// Flag to indicate if connected to WiFi
bool connectedToWiFi = false;

bool setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  
  // Try first with RGB565, which works on more cameras
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;  // 320x240
  config.jpeg_quality = 12;
  config.fb_count = 2;
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x (RGB565, QVGA)\n", err);
    
    // Try with YUV format next
    config.pixel_format = PIXFORMAT_YUV422;
    err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x (YUV422, QVGA)\n", err);
      
      // Last try with JPEG format
      config.pixel_format = PIXFORMAT_JPEG;
      err = esp_camera_init(&config);
      if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x (JPEG, QVGA)\n", err);
        return false;
      }
    }
  }
  
  Serial.println("Camera initialized successfully!");
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    s->set_saturation(s, -2);  // Lower saturation
    s->set_brightness(s, 1);   // Increase brightness slightly
    s->set_contrast(s, 1);     // Increase contrast
  }
  return true;
}

// Function to attempt connecting to WiFi hotspot
bool connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(wifi_ssid);
  
  WiFi.begin(wifi_ssid, wifi_password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < MAX_CONNECTION_ATTEMPTS) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected successfully!");
    Serial.print("ESP32-CAM IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println();
    Serial.println("Failed to connect to WiFi. Falling back to AP mode only.");
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("ESP32-CAM Stream Vision Starting...");
  
  // First try to connect to WiFi in client mode
  WiFi.mode(WIFI_STA);
  connectedToWiFi = connectToWiFi();
  
  if (connectedToWiFi) {
    // If connected successfully, set up for both AP and STA modes
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.println("Running in both Client and AP modes");
  } else {
    // If failed to connect, set up AP mode only
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.println("Running in AP mode only");
  }
  
  Serial.print("Access Point created: ");
  Serial.println(ap_ssid);
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());
  
  // Try to initialize camera - add a delay to let power stabilize
  delay(100);
  cameraInitialized = setupCamera();
  if (!cameraInitialized) {
    Serial.println("Failed to initialize camera. Will continue without camera functionality.");
  } else {
    Serial.println("Camera is working and ready for streaming.");
  }
  
  // Define server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/auto", HTTP_GET, handleAutoRefresh);
  server.on("/sensor-data", HTTP_GET, handleSensorData);
  server.on("/update-sensor", HTTP_POST, handleUpdateSensor);
  server.on("/test", HTTP_GET, handleTestPage);
  server.on("/network-status", HTTP_GET, handleNetworkStatus);
  
  // Only add stream endpoint if camera was initialized
  if (cameraInitialized) {
    server.on("/stream", HTTP_GET, handleStream);
    Serial.println("Camera stream available at /stream");
  }
  
  // Start the server
  server.begin();
  Serial.println("HTTP server started");
  
  // Initialize default sensor values
  sensorData.temperature = 25.0;
  sensorData.humidity = 50.0;
  sensorData.waterSensor = 0;
}

void loop() {
  server.handleClient();
  
  // Check WiFi connection status periodically and attempt reconnection if needed
  if (connectedToWiFi && WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    WiFi.reconnect();
    
    // Wait a bit for reconnection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println("WiFi reconnected successfully!");
      Serial.print("ESP32-CAM IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println();
      Serial.println("Failed to reconnect to WiFi. Operating in AP mode only.");
      connectedToWiFi = false;
    }
  }
  
  delay(1); // Small delay to prevent watchdog issues
}

// Network status endpoint to check connectivity
void handleNetworkStatus() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32-CAM Network Status</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:20px;line-height:1.6;}";
  html += "h1{color:#0066cc;}";
  html += ".info{background:#f0f0f0;padding:15px;border-radius:5px;margin:10px 0;}";
  html += ".status{font-weight:bold;}";
  html += ".connected{color:green;}";
  html += ".not-connected{color:red;}";
  html += "</style>";
  html += "</head><body>";
  
  html += "<h1>ESP32-CAM Network Status</h1>";
  
  html += "<div class='info'>";
  html += "<p>WiFi Client Mode: <span class='status " + String(connectedToWiFi ? "connected" : "not-connected") + "'>";
  html += connectedToWiFi ? "Connected" : "Not Connected";
  html += "</span></p>";
  
  if (connectedToWiFi) {
    html += "<p>Connected to: " + String(wifi_ssid) + "</p>";
    html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
    html += "<p>Signal Strength (RSSI): " + String(WiFi.RSSI()) + " dBm</p>";
  }
  
  html += "<p>Access Point: <span class='status connected'>Active</span></p>";
  html += "<p>AP SSID: " + String(ap_ssid) + "</p>";
  html += "<p>AP IP Address: " + WiFi.softAPIP().toString() + "</p>";
  
  html += "</div>";
  
  html += "<p><a href='/'>Return to Dashboard</a></p>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Camera stream handler
void handleStream() {
  WiFiClient client = server.client();
  
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      delay(100);
      continue;
    }
    
    uint8_t *jpg_buf = NULL;
    size_t jpg_size = 0;
    
    if (fb->format != PIXFORMAT_JPEG) {
      // Convert to JPEG if needed
      bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_size);
      esp_camera_fb_return(fb);
      fb = NULL;
      
      if (!jpeg_converted) {
        Serial.println("JPEG conversion failed");
        delay(100);
        continue;
      }
    } else {
      // If already JPEG, use buffer directly
      jpg_buf = fb->buf;
      jpg_size = fb->len;
    }
    
    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n";
    response += "Content-Length: " + String(jpg_size) + "\r\n\r\n";
    client.print(response);
    
    // Send JPEG data in chunks to prevent buffer issues
    const size_t bufSize = 4096;
    for (size_t i = 0; i < jpg_size; i += bufSize) {
      size_t remainder = jpg_size - i;
      size_t toWrite = (remainder > bufSize) ? bufSize : remainder;
      client.write(jpg_buf + i, toWrite);
    }
    
    client.print("\r\n");
    
    // Clean up
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
    } else if (jpg_buf && fb == NULL) {
      free(jpg_buf);
      jpg_buf = NULL;
    }
    
    delay(100);
  }
}

// Handler to get sensor data as JSON
void handleSensorData() {
  // Log request
  Serial.println("Sensor data requested by client");
  
  // Add CORS headers to allow access from any origin
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  
  String jsonData = "{";
  jsonData += "\"temperature\":" + String(sensorData.temperature) + ",";
  jsonData += "\"humidity\":" + String(sensorData.humidity) + ",";
  jsonData += "\"waterSensor\":" + String(sensorData.waterSensor) + ",";
  jsonData += "\"timestamp\":" + String(lastDataReceived) + ",";
  jsonData += "\"serverTime\":" + String(millis()) + ",";
  jsonData += "\"cameraAvailable\":" + String(cameraInitialized ? "true" : "false") + ",";
  jsonData += "\"wifiConnected\":" + String(connectedToWiFi ? "true" : "false");
  if (connectedToWiFi) {
    jsonData += ",\"clientIP\":\"" + WiFi.localIP().toString() + "\"";
    jsonData += ",\"signalStrength\":" + String(WiFi.RSSI());
  }
  jsonData += "}";
  
  // Log response data
  Serial.print("Sending data: ");
  Serial.println(jsonData);
  
  server.send(200, "application/json", jsonData);
}

// Handler to update sensor data (called by ESP8266)
void handleUpdateSensor() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    // Log received data
    Serial.println("Received update: " + body);
    
    bool dataUpdated = false;
    
    // Parse JSON without library
    
    // Find temperature
    int tempStart = body.indexOf("\"temperature\":") + 14;
    int tempEnd = body.indexOf(",", tempStart);
    if (tempStart > 14 && tempEnd > tempStart) {
      sensorData.temperature = body.substring(tempStart, tempEnd).toFloat();
      dataUpdated = true;
    }
    
    // Find humidity
    int humStart = body.indexOf("\"humidity\":") + 11;
    int humEnd = body.indexOf(",", humStart);
    if (humStart > 11 && humEnd > humStart) {
      sensorData.humidity = body.substring(humStart, humEnd).toFloat();
      dataUpdated = true;
    }
    
    // Find water sensor
    int waterStart = body.indexOf("\"waterSensor\":") + 14;
    int waterEnd = body.indexOf("}", waterStart);
    if (waterStart > 14 && waterEnd > waterStart) {
      sensorData.waterSensor = body.substring(waterStart, waterEnd).toInt();
      dataUpdated = true;
    }
    
    // Update timestamp only if we processed new data
    if (dataUpdated) {
      lastDataReceived = millis();
      
      Serial.println("Sensor data updated via HTTP:");
      Serial.print("Temperature: ");
      Serial.print(sensorData.temperature);
      Serial.print(" °C, Humidity: ");
      Serial.print(sensorData.humidity);
      Serial.print(" %, Water: ");
      Serial.println(sensorData.waterSensor ? "DETECTED" : "DRY");
      
      server.send(200, "application/json", "{\"status\":\"ok\",\"timestamp\":" + String(lastDataReceived) + "}");
    } else {
      Serial.println("Error: Failed to parse update data");
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid data format\"}");
    }
  } else {
    Serial.println("Error: No data received in update request");
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data received\"}");
  }
}

// Simple test page with auto refresh
void handleTestPage() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32 Test Page</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='3'>";  // Auto-refresh every 3 seconds
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:20px;line-height:1.6;}";
  html += "h1{color:#0066cc;}";
  html += ".value{font-weight:bold;font-size:20px;}";
  html += ".timestamp{color:#666;font-size:12px;margin-top:20px;}";
  html += "</style>";
  html += "</head><body>";
  
  html += "<h1>ESP32 Test Page (Auto-refresh)</h1>";
  html += "<p>Temperature: <span class='value'>" + String(sensorData.temperature, 1) + " °C</span></p>";
  html += "<p>Humidity: <span class='value'>" + String(sensorData.humidity, 1) + " %</span></p>";
  html += "<p>Water Sensor: <span class='value'>" + String(sensorData.waterSensor ? "DETECTED" : "DRY") + "</span></p>";
  html += "<p class='timestamp'>Last Update: " + String(lastDataReceived) + "</p>";
  html += "<p class='timestamp'>Server Time: " + String(millis()) + "</p>";
  
  // Add network status information
  html += "<p>Network Mode: " + String(connectedToWiFi ? "WiFi Client + AP" : "AP Only") + "</p>";
  if (connectedToWiFi) {
    html += "<p>Client IP: " + WiFi.localIP().toString() + "</p>";
  }
  html += "<p>AP IP: " + WiFi.softAPIP().toString() + "</p>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Simple page with auto-refresh meta tag
void handleAutoRefresh() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32 Sensor Dashboard</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='3'>";  // Auto-refresh every 3 seconds
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f0f0;}";
  html += ".dashboard{max-width:800px;margin:0 auto;}";
  html += ".header{text-align:center;margin-bottom:30px;}";
  html += ".sensor-grid{display:grid;grid-template-columns:repeat(auto-fit, minmax(200px, 1fr));gap:20px;}";
  html += ".sensor-item{background:white;border-radius:10px;padding:20px;text-align:center;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
  html += ".sensor-value{font-size:2rem;font-weight:bold;margin:10px 0;}";
  html += ".temp{color:#e74c3c;}";
  html += ".humidity{color:#3498db;}";
  html += ".water-dry{color:#2ecc71;}";
  html += ".water-detected{color:#e74c3c;}";
  html += ".camera-container{margin:20px 0;border-radius:10px;overflow:hidden;}";
  html += ".camera-container img{width:100%;display:block;}";
  html += ".timestamp{text-align:center;margin-top:20px;color:#666;font-size:12px;}";
  html += ".network-status{background:white;border-radius:10px;padding:10px;margin-top:20px;text-align:center;}";
  html += "</style>";
  html += "</head><body>";
  
  html += "<div class='dashboard'>";
  html += "<div class='header'>";
  html += "<h1>ESP32 Sensor Dashboard</h1>";
  html += "<p>Auto-refreshes every 3 seconds</p>";
  html += "</div>";
  
  // Add camera feed if available
  if (cameraInitialized) {
    html += "<div class='camera-container'>";
    html += "<img src='/stream' alt='Camera Feed'>";
    html += "</div>";
  }
  
  html += "<div class='sensor-grid'>";
  
  html += "<div class='sensor-item'>";
  html += "<h2>Temperature</h2>";
  html += "<div class='sensor-value temp'>" + String(sensorData.temperature, 1) + " °C</div>";
  html += "</div>";
  
  html += "<div class='sensor-item'>";
  html += "<h2>Humidity</h2>";
  html += "<div class='sensor-value humidity'>" + String(sensorData.humidity, 1) + " %</div>";
  html += "</div>";
  
  html += "<div class='sensor-item'>";
  html += "<h2>Water Sensor</h2>";
  html += "<div class='sensor-value " + String(sensorData.waterSensor ? "water-detected" : "water-dry") + "'>";
  html += sensorData.waterSensor ? "DETECTED" : "DRY";
  html += "</div>";
  html += "</div>";
  
  html += "</div>"; // End sensor-grid
  
  // Add network status information
  html += "<div class='network-status'>";
  html += "<h2>Network Status</h2>";
  html += "<p>Mode: " + String(connectedToWiFi ? "WiFi Client + AP" : "AP Only") + "</p>";
  if (connectedToWiFi) {
    html += "<p>Client IP: " + WiFi.localIP().toString() + "</p>";
    html += "<p>Client SSID: " + String(wifi_ssid) + "</p>";
  }
  html += "<p>AP IP: " + WiFi.softAPIP().toString() + "</p>";
  html += "<p>AP SSID: " + String(ap_ssid) + "</p>";
  html += "<p><a href='/network-status'>Detailed Network Info</a></p>";
  html += "</div>";
  
  html += "<div class='timestamp'>";
  html += "Last Update: " + String(lastDataReceived) + "<br>";
  html += "Server Time: " + String(millis()) + "<br>";
  html += "Page Generated: " + String(millis());
  html += "</div>";
  
  html += "</div>"; // End dashboard
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Full dashboard with JavaScript-based updates
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32 Sensor Dashboard</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f0f0;}";
  html += ".dashboard{max-width:800px;margin:0 auto;}";
  html += ".header{text-align:center;margin-bottom:30px;}";
  html += ".sensor-grid{display:grid;grid-template-columns:repeat(auto-fit, minmax(200px, 1fr));gap:20px;}";
  html += ".sensor-item{background:white;border-radius:10px;padding:20px;text-align:center;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
  html += ".sensor-value{font-size:2rem;font-weight:bold;margin:10px 0;}";
  html += ".temp{color:#e74c3c;}";
  html += ".humidity{color:#3498db;}";
  html += ".water-dry{color:#2ecc71;}";
  html += ".water-detected{color:#e74c3c;}";
  html += ".camera-container{margin:20px 0;border-radius:10px;overflow:hidden;background:#000;}";
  html += ".camera-container img{width:100%;display:block;}";
  html += ".controls{display:flex;justify-content:center;margin:20px 0;flex-wrap:wrap;}";
  html += ".button{background:#3498db;color:white;border:none;padding:10px 20px;border-radius:5px;cursor:pointer;font-size:16px;margin:5px 10px;}";
  html += ".button:hover{background:#2980b9;}";
  html += ".timestamp{text-align:center;margin-top:20px;color:#666;font-size:12px;}";
  html += ".network-info{background:white;border-radius:10px;padding:15px;margin-top:20px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
  html += ".network-status{margin-bottom:10px;}";
  html += ".connected{color:green;font-weight:bold;}";
  html += ".disconnected{color:red;font-weight:bold;}";
  html += "</style>";
  html += "</head><body>";
  
  html += "<div class='dashboard'>";
  html += "<div class='header'>";
  html += "<h1>ESP32 Sensor Dashboard</h1>";
  html += "<p>Real-time monitoring system</p>";
  html += "</div>";
  
  // Add camera feed if available
  if (cameraInitialized) {
    html += "<div class='camera-container'>";
    html += "<img src='/stream' id='camera-feed' alt='Camera Feed'>";
    html += "</div>";
  }
  
  html += "<div class='sensor-grid'>";
  
  html += "<div class='sensor-item'>";
  html += "<h2>Temperature</h2>";
  html += "<div class='sensor-value temp' id='temperature'>" + String(sensorData.temperature, 1) + " °C</div>";
  html += "</div>";
  
  html += "<div class='sensor-item'>";
  html += "<h2>Humidity</h2>";
  html += "<div class='sensor-value humidity' id='humidity'>" + String(sensorData.humidity, 1) + " %</div>";
  html += "</div>";
  
  html += "<div class='sensor-item'>";
  html += "<h2>Water Sensor</h2>";
  html += "<div class='sensor-value " + String(sensorData.waterSensor ? "water-detected" : "water-dry") + "' id='water-status'>";
  html += sensorData.waterSensor ? "DETECTED" : "DRY";
  html += "</div>";
  html += "</div>";
  
  html += "</div>"; // End sensor-grid
  
  // Add network status information
  html += "<div class='network-info'>";
  html += "<h2>Network Status</h2>";
  html += "<div class='network-status'>";
  html += "WiFi Client: <span class='" + String(connectedToWiFi ? "connected" : "disconnected") + "' id='wifi-status'>";
  html += connectedToWiFi ? "Connected to " + String(wifi_ssid) : "Disconnected";
  html += "</span>";
  html += "</div>";
  
  if (connectedToWiFi) {
    html += "<p id='client-ip'>IP: " + WiFi.localIP().toString() + "</p>";
  }
  
  html += "<p>Access Point: <span class='connected'>Active</span></p>";
  html += "<p>AP SSID: " + String(ap_ssid) + "</p>";
  html += "<p id='ap-ip'>AP IP: " + WiFi.softAPIP().toString() + "</p>";
  html += "<p><a href='/network-status' class='button' style='display:inline-block;'>Detailed Network Info</a></p>";
  html += "</div>";
  
  html += "<div class='controls'>";
  html += "<button class='button' onclick='refreshData()'>Refresh Data</button>";
  html += "<button class='button' onclick='autoRefresh(1)'>Auto-refresh (1s)</button>";
  html += "<button class='button' onclick='autoRefresh(3)'>Auto-refresh (3s)</button>";
  html += "<button class='button' onclick='stopAutoRefresh()'>Stop Auto-refresh</button>";
  html += "</div>";
  
  html += "<div class='timestamp' id='timestamp'>";
  html += "Last Update: " + String(lastDataReceived) + "<br>";
  html += "Server Time: " + String(millis());
  html += "</div>";
  
  html += "</div>"; // End dashboard
  
  html += "<script>";
  
  // Very simple fetch and update function
  html += "var refreshTimer;";
  
  html += "function refreshData() {";
  html += "  fetch('/sensor-data?' + Date.now())";
  html += "    .then(function(response) { return response.json(); })";
  html += "    .then(function(data) {";
  html += "      document.getElementById('temperature').innerHTML = data.temperature.toFixed(1) + ' °C';";
  html += "      document.getElementById('humidity').innerHTML = data.humidity.toFixed(1) + ' %';";
  
  html += "      var waterStatus = document.getElementById('water-status');";
  html += "      waterStatus.innerHTML = data.waterSensor ? 'DETECTED' : 'DRY';";
  html += "      waterStatus.className = 'sensor-value ' + (data.waterSensor ? 'water-detected' : 'water-dry');";
  
  html += "      document.getElementById('timestamp').innerHTML = 'Last Update: ' + data.timestamp + '<br>Server Time: ' + data.serverTime + '<br>Client Time: ' + Date.now();";
  html += "    })";
  html += "    .catch(function(error) {";
  html += "      console.error('Error:', error);";
  html += "    });";
  html += "}";
  
  // Auto-refresh functions
  html += "function autoRefresh(seconds) {";
  html += "  stopAutoRefresh();";
  html += "  refreshData();";
  html += "  refreshTimer = setInterval(refreshData, seconds * 1000);";
  html += "}";
  
  html += "function stopAutoRefresh() {";
  html += "  if (refreshTimer) {";
  html += "    clearInterval(refreshTimer);";
  html += "    refreshTimer = null;";
  html += "  }";
  html += "}";
  
  // Initialize with auto-refresh
  html += "document.addEventListener('DOMContentLoaded', function() {";
  html += "  refreshData();";
  html += "  autoRefresh(1);";
  html += "});";
  
  html += "</script>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}
