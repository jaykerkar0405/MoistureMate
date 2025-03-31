#include <time.h>
#include "secrets.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

// Pin definitions
const int PUMP_RELAY_PIN = D1;
const int MOISTURE_SENSOR_PIN = A0;

// Operation mode constants
#define AUTO_MODE true
#define MANUAL_MODE false

// Log level definitions
#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_WARNING 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3
int currentLogLevel = LOG_LEVEL_INFO; // Default log level

// System variables
bool pumpRunning = false;
int moistureThreshold = 30;
bool operationMode = AUTO_MODE;
unsigned long pumpStartTime = 0;
unsigned long lastPublishTime = 0;
const int publishInterval = 60000;
const unsigned long maxPumpRuntime = 30000;

// WiFi & MQTT Clients
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// Global certificate objects
BearSSL::X509List cert(AWS_CERT_CA);
BearSSL::X509List client_crt(AWS_CERT_CRT);
BearSSL::PrivateKey key(AWS_CERT_PRIVATE);

// Function prototypes
void connectToAWS();
void connectToWiFi();
void publishStatus();
String getTimestamp();
void checkPumpTimeout();
int readMoistureSensor();
void activatePump(bool state);
void messageHandler(char* topic, byte* payload, unsigned int length);
void logMessage(int level, const char* module, const char* message);
void logMessagef(int level, const char* module, const char* format, ...);

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000); // Allow serial to initialize

  // Set pin modes
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(MOISTURE_SENSOR_PIN, INPUT);

  // Ensure pump is off at startup (active LOW relay)
  digitalWrite(PUMP_RELAY_PIN, HIGH);

  // Print header with version info
  Serial.println();
  Serial.println("╔════════════════════════════════════════════════════╗");
  Serial.println("║          ESP8266 AWS IoT Plant Watering System     ║");
  Serial.println("║                     Version 1.0                    ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  
  logMessage(LOG_LEVEL_INFO, "INIT", "System starting up...");

  // Connect to WiFi
  connectToWiFi();

  // Configure SSL/TLS certificates
  wifiClient.setTrustAnchors(&cert);
  wifiClient.setClientRSACert(&client_crt, &key);

  // Set time via NTP - required for X.509 certificate validation
  logMessage(LOG_LEVEL_INFO, "TIME", "Synchronizing with NTP servers...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  time_t now = time(nullptr);
  int dots = 0;
  while (now < 8 * 3600 * 2) {
    delay(500);
    if (dots % 20 == 0) {
      Serial.println();
      Serial.print("[TIME] Waiting for NTP sync: ");
    }
    Serial.print(".");
    dots++;
    now = time(nullptr);
  }
  Serial.println();
  
  char timeBuffer[25];
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  sprintf(timeBuffer, "%04d-%02d-%02d %02d:%02d:%02d UTC",
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  logMessagef(LOG_LEVEL_INFO, "TIME", "Time synchronized: %s", timeBuffer);

  // Set MQTT client
  mqttClient.setServer(AWS_ENDPOINT, 8883);
  mqttClient.setCallback(messageHandler);

  // Connect to AWS IoT
  connectToAWS();

  logMessage(LOG_LEVEL_INFO, "INIT", "System initialization complete");
  logMessagef(LOG_LEVEL_INFO, "CONF", "Current settings: Mode=%s, Threshold=%d%, MaxPumpTime=%ds", 
              operationMode ? "AUTO" : "MANUAL", moistureThreshold, maxPumpRuntime/1000);
  
  Serial.println("╔════════════════════════════════════════════════════╗");
  Serial.println("║                 System is running                  ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    logMessage(LOG_LEVEL_ERROR, "WIFI", "Connection lost. Reconnecting...");
    connectToWiFi();
  }

  // Check MQTT connection
  if (!mqttClient.connected()) {
    connectToAWS();
  }

  // Process MQTT messages
  mqttClient.loop();

  // Read moisture level
  int moistureLevel = readMoistureSensor();

  // Automatic mode logic
  if (operationMode == AUTO_MODE) {
    // If moisture is below threshold and pump is not running, turn it on
    if (moistureLevel < moistureThreshold && !pumpRunning) {
      logMessagef(LOG_LEVEL_INFO, "AUTO", "Moisture low (%d%% < %d%%). Activating pump.", 
                 moistureLevel, moistureThreshold);
      activatePump(true);
      pumpStartTime = millis();
    }

    // If moisture is above threshold and pump is running, turn it off
    else if (moistureLevel >= moistureThreshold && pumpRunning) {
      logMessagef(LOG_LEVEL_INFO, "AUTO", "Moisture restored (%d%% >= %d%%). Deactivating pump.", 
                 moistureLevel, moistureThreshold);
      activatePump(false);
    }
  }

  // Check pump timeout (safety feature)
  checkPumpTimeout();

  // Publish sensor data periodically
  if (millis() - lastPublishTime > publishInterval) {
    publishStatus();
    lastPublishTime = millis();
  }

  delay(1000);
}

// Connect to WiFi network
void connectToWiFi() {
  logMessagef(LOG_LEVEL_INFO, "WIFI", "Connecting to network: %s", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (attempts % 20 == 0) {
      Serial.println();
      Serial.print("[WIFI] Connection attempt: ");
    }
    delay(500);
    Serial.print(".");
    attempts++;
    
    // After 60 seconds (120 attempts), restart connection process
    if (attempts > 120) {
      logMessage(LOG_LEVEL_ERROR, "WIFI", "Connection timeout. Retrying from scratch...");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      attempts = 0;
    }
  }

  char ipBuffer[20];
  sprintf(ipBuffer, "%d.%d.%d.%d", 
          WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  
  logMessage(LOG_LEVEL_INFO, "WIFI", "Connection established");
  logMessagef(LOG_LEVEL_INFO, "WIFI", "IP address: %s", ipBuffer);
  logMessagef(LOG_LEVEL_DEBUG, "WIFI", "Signal strength: %d dBm", WiFi.RSSI());
}

// Connect to AWS IoT Core
void connectToAWS() {
  logMessage(LOG_LEVEL_INFO, "MQTT", "Connecting to AWS IoT Core...");

  int attempts = 0;
  while (!mqttClient.connected()) {
    if (mqttClient.connect(CLIENT_ID)) {
      logMessage(LOG_LEVEL_INFO, "MQTT", "Connected to AWS IoT Core");

      // Subscribe to control topic
      mqttClient.subscribe(CONTROL_TOPIC);
      logMessagef(LOG_LEVEL_INFO, "MQTT", "Subscribed to topic: %s", CONTROL_TOPIC);

      // Publish initial status
      publishStatus();
    } else {
      int state = mqttClient.state();
      
      // Map error codes to readable messages
      const char* errorMsg;
      switch(state) {
        case -4: errorMsg = "Connection timeout"; break;
        case -3: errorMsg = "Connection lost"; break;
        case -2: errorMsg = "Connect failed"; break;
        case -1: errorMsg = "Client disconnected"; break;
        case 1: errorMsg = "Bad protocol"; break;
        case 2: errorMsg = "Bad client ID"; break;
        case 3: errorMsg = "Server unavailable"; break;
        case 4: errorMsg = "Bad credentials"; break;
        case 5: errorMsg = "Not authorized"; break;
        default: errorMsg = "Unknown error"; break;
      }
      
      logMessagef(LOG_LEVEL_ERROR, "MQTT", "Connection failed: %s (code: %d)", errorMsg, state);
      
      attempts++;
      if (attempts % 3 == 0) {
        logMessagef(LOG_LEVEL_WARNING, "MQTT", "Multiple connection failures. Check credentials and network.");
      }
      
      delay(5000);
    }
  }
}

// MQTT message handler
void messageHandler(char* topic, byte* payload, unsigned int length) {
  // Create a buffer for the payload
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  
  logMessagef(LOG_LEVEL_DEBUG, "MQTT", "Message received on topic: %s", topic);
  logMessagef(LOG_LEVEL_DEBUG, "MQTT", "Payload: %s", message);

  // Parse JSON
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    logMessagef(LOG_LEVEL_ERROR, "JSON", "Parsing failed: %s", error.c_str());
    return;
  }

  // Process commands
  const char* command = doc["command"];

  if (strcmp(command, "pump") == 0) {
    const char* state = doc["state"];

    if (operationMode == MANUAL_MODE) {
      if (strcmp(state, "on") == 0) {
        logMessage(LOG_LEVEL_INFO, "MANUAL", "Pump activation command received");
        activatePump(true);
        pumpStartTime = millis();
      } else if (strcmp(state, "off") == 0) {
        logMessage(LOG_LEVEL_INFO, "MANUAL", "Pump deactivation command received");
        activatePump(false);
      }
    } else {
      logMessage(LOG_LEVEL_WARNING, "CMD", "Pump command ignored (system in AUTO mode)");
    }
  } else if (strcmp(command, "mode") == 0) {
    const char* mode = doc["state"];

    if (strcmp(mode, "auto") == 0) {
      operationMode = AUTO_MODE;
      logMessage(LOG_LEVEL_INFO, "MODE", "Switched to AUTO mode");

      // If pump was running in manual mode, turn it off
      if (pumpRunning) {
        logMessage(LOG_LEVEL_INFO, "MODE", "Auto-disabling pump due to mode change");
        activatePump(false);
      }
    } else if (strcmp(mode, "manual") == 0) {
      operationMode = MANUAL_MODE;
      logMessage(LOG_LEVEL_INFO, "MODE", "Switched to MANUAL mode");

      // If pump was running in auto mode, turn it off
      if (pumpRunning) {
        logMessage(LOG_LEVEL_INFO, "MODE", "Auto-disabling pump due to mode change");
        activatePump(false);
      }
    }
  } else if (strcmp(command, "threshold") == 0) {
    int newThreshold = doc["value"];
    if (newThreshold >= 0 && newThreshold <= 100) {
      int oldThreshold = moistureThreshold;
      moistureThreshold = newThreshold;
      logMessagef(LOG_LEVEL_INFO, "CONF", "Moisture threshold updated: %d%% -> %d%%", 
                 oldThreshold, moistureThreshold);
    } else {
      logMessagef(LOG_LEVEL_WARNING, "CONF", "Invalid threshold value received: %d (valid range: 0-100)", 
                 newThreshold);
    }
  } else if (strcmp(command, "log_level") == 0) {
    int newLevel = doc["level"];
    if (newLevel >= LOG_LEVEL_ERROR && newLevel <= LOG_LEVEL_DEBUG) {
      int oldLevel = currentLogLevel;
      currentLogLevel = newLevel;
      
      const char* levelNames[] = {"ERROR", "WARNING", "INFO", "DEBUG"};
      logMessagef(LOG_LEVEL_INFO, "CONF", "Log level changed: %s -> %s", 
                 levelNames[oldLevel], levelNames[currentLogLevel]);
    }
  } else {
    logMessagef(LOG_LEVEL_WARNING, "CMD", "Unknown command received: %s", command);
  }

  // Publish updated status after command
  publishStatus();
}

// Read moisture sensor and convert to percentage
int readMoistureSensor() {
  int wetValue = 300;
  int dryValue = 1023;
  int rawValue = analogRead(MOISTURE_SENSOR_PIN);

  // Calculate percentage (inverted: 0% = dry, 100% = wet)
  int moisturePercentage = map(rawValue, dryValue, wetValue, 0, 100);

  // Constrain to valid range
  moisturePercentage = constrain(moisturePercentage, 0, 100);

  logMessagef(LOG_LEVEL_DEBUG, "SENSOR", "Raw moisture value: %d", rawValue);
  logMessagef(LOG_LEVEL_DEBUG, "SENSOR", "Moisture percentage: %d%%", moisturePercentage);

  return moisturePercentage;
}

// Control the water pump - MODIFIED FOR ACTIVE LOW RELAY
void activatePump(bool state) {
  if (state) {
    digitalWrite(PUMP_RELAY_PIN, LOW);  // Active LOW relay
    pumpRunning = true;
    logMessagef(LOG_LEVEL_INFO, "PUMP", "Activated at %s", getTimestamp().c_str());
  } else {
    digitalWrite(PUMP_RELAY_PIN, HIGH);  // Active LOW relay
    pumpRunning = false;
    
    // If pump was running, calculate how long it was on
    if (pumpStartTime > 0) {
      unsigned long pumpRuntime = millis() - pumpStartTime;
      logMessagef(LOG_LEVEL_INFO, "PUMP", "Deactivated. Run time: %.1f seconds", pumpRuntime/1000.0);
      pumpStartTime = 0;
    } else {
      logMessage(LOG_LEVEL_INFO, "PUMP", "Deactivated");
    }
  }
}

// Publish system status to AWS IoT
void publishStatus() {
  // Read current moisture level
  int moistureLevel = readMoistureSensor();

  // Create JSON document
  StaticJsonDocument<200> doc;

  doc["deviceId"] = CLIENT_ID;
  doc["moisture"] = moistureLevel;
  doc["timestamp"] = getTimestamp();
  doc["threshold"] = moistureThreshold;
  doc["pumpState"] = pumpRunning ? "on" : "off";
  doc["mode"] = operationMode ? "auto" : "manual";
  doc["uptime"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();

  // Serialize JSON to string
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  logMessage(LOG_LEVEL_DEBUG, "MQTT", "Publishing status to AWS IoT");
  
  // Publish to AWS IoT
  if (mqttClient.publish(STATUS_TOPIC, jsonBuffer)) {
    logMessage(LOG_LEVEL_INFO, "MQTT", "Status published successfully");
    logMessagef(LOG_LEVEL_DEBUG, "MQTT", "Status payload: %s", jsonBuffer);
  } else {
    logMessage(LOG_LEVEL_ERROR, "MQTT", "Failed to publish status");
  }
}

// Get current timestamp from system time
String getTimestamp() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  char timeString[24];
  sprintf(timeString, "%04d-%02d-%02dT%02d:%02d:%02dZ",
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  return String(timeString);
}

// Safety feature to prevent pump from running too long
void checkPumpTimeout() {
  if (pumpRunning && (millis() - pumpStartTime > maxPumpRuntime)) {
    logMessagef(LOG_LEVEL_WARNING, "SAFETY", "Pump timeout reached (%d seconds). Force stopping.", 
               maxPumpRuntime/1000);
    activatePump(false);
  }
}

// Logging with level control and consistent formatting
void logMessage(int level, const char* module, const char* message) {
  // Skip if message level is higher than current log level
  if (level > currentLogLevel) return;
  
  // Level labels
  const char* levelLabels[] = {"ERROR", "WARN ", "INFO ", "DEBUG"};
  
  // Format: [TIME][LEVEL][MODULE] Message
  Serial.print("[");
  Serial.print(getTimestamp());
  Serial.print("][");
  Serial.print(levelLabels[level]);
  Serial.print("][");
  Serial.print(module);
  Serial.print("] ");
  Serial.println(message);
}

// Formatted logging with variable arguments
void logMessagef(int level, const char* module, const char* format, ...) {
  // Skip if message level is higher than current log level
  if (level > currentLogLevel) return;
  
  // Format the message using sprintf-style formatting
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  // Use the base logging function
  logMessage(level, module, buffer);
}
