#include <time.h>
#include "secrets.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

const int PUMP_RELAY_PIN = D1;
const int MOISTURE_SENSOR_PIN = A0;

#define AUTO_MODE true
#define MANUAL_MODE false

#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_DEBUG 3
#define LOG_LEVEL_WARNING 1
int currentLogLevel = LOG_LEVEL_INFO;

bool pumpRunning = false;
int moistureThreshold = 30;
bool operationMode = AUTO_MODE;
unsigned long lastLoopTime = 0;
unsigned long pumpStartTime = 0;
unsigned long lastPublishTime = 0;
const int publishInterval = 60000;
const unsigned long maxPumpRuntime = 30000;

int lastMoistureLevel = -1;
int moistureChangeThreshold = 2;

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

BearSSL::X509List cert(AWS_CERT_CA);
BearSSL::X509List client_crt(AWS_CERT_CRT);
BearSSL::PrivateKey key(AWS_CERT_PRIVATE);

void connectToAWS();
void connectToWiFi();
void publishStatus();
String getTimestamp();
void checkPumpTimeout();
int readMoistureSensor();
void activatePump(bool state);
void printProgressBar(int percent, int width);
void logMessage(int level, const char* module, const char* message);
void messageHandler(char* topic, byte* payload, unsigned int length);
void printLogBox(int level, const char* module, const char* message);
void logMessagef(int level, const char* module, const char* format, ...);

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(MOISTURE_SENSOR_PIN, INPUT);
  digitalWrite(PUMP_RELAY_PIN, HIGH);

  Serial.println();
  Serial.println("╔════════════════════════════════════════════════════════════════╗");
  Serial.println("║                                                                ║");
  Serial.println("║            ESP8266 AWS IoT Plant Watering System               ║");
  Serial.println("║                         Version 1.2                            ║");
  Serial.println("║                                                                ║");
  Serial.println("╚════════════════════════════════════════════════════════════════╝");

  logMessage(LOG_LEVEL_INFO, "INIT", "System starting up...");

  connectToWiFi();

  wifiClient.setTrustAnchors(&cert);
  wifiClient.setClientRSACert(&client_crt, &key);

  logMessage(LOG_LEVEL_INFO, "TIME", "Synchronizing with NTP servers...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  int dots = 0;

  Serial.println("┌────────────────────────────────────────────────────┐");
  Serial.print("│ NTP Synchronization ");

  while (now < 8 * 3600 * 2) {
    delay(100);
    if (dots % 20 == 0) {
      Serial.print("\n│ ");
    }
    Serial.print("▫");
    dots++;
    now = time(nullptr);
  }

  Serial.println("\n└────────────────────────────────────────────────────┘");

  char timeBuffer[25];
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  sprintf(timeBuffer, "%04d-%02d-%02d %02d:%02d:%02d UTC",
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  logMessagef(LOG_LEVEL_INFO, "TIME", "Time synchronized: %s", timeBuffer);

  mqttClient.setServer(AWS_ENDPOINT, 8883);
  mqttClient.setCallback(messageHandler);

  connectToAWS();

  logMessage(LOG_LEVEL_INFO, "INIT", "System initialization complete");
  logMessagef(LOG_LEVEL_INFO, "CONF", "Current settings: Mode=%s, Threshold=%d%, MaxPumpTime=%ds, MoistureChangeThreshold=%d%",
              operationMode ? "AUTO" : "MANUAL", moistureThreshold, maxPumpRuntime / 1000, moistureChangeThreshold);

  Serial.println("╔════════════════════════════════════════════════════════════════╗");
  Serial.println("║                                                                ║");
  Serial.println("║                      System is running                         ║");
  Serial.println("║                                                                ║");
  Serial.println("╚════════════════════════════════════════════════════════════════╝");

  lastLoopTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  if (WiFi.status() != WL_CONNECTED) {
    logMessage(LOG_LEVEL_ERROR, "WIFI", "Connection lost. Reconnecting...");
    connectToWiFi();
  }

  if (!mqttClient.connected()) {
    connectToAWS();
  }

  mqttClient.loop();

  static unsigned long lastSensorRead = 0;
  static int moistureLevel = 0;

  if (currentMillis - lastSensorRead >= 100) {
    moistureLevel = readMoistureSensor();
    lastSensorRead = currentMillis;

    if (lastMoistureLevel == -1 || abs(moistureLevel - lastMoistureLevel) >= moistureChangeThreshold) {
      logMessagef(LOG_LEVEL_INFO, "SENSOR", "Moisture change detected: %d%% → %d%%",
                  lastMoistureLevel, moistureLevel);

      publishStatus();
      lastMoistureLevel = moistureLevel;
    }
  }

  if (operationMode == AUTO_MODE) {
    if (moistureLevel < moistureThreshold && !pumpRunning) {
      logMessagef(LOG_LEVEL_INFO, "AUTO", "Moisture low (%d%% < %d%%). Activating pump.",
                  moistureLevel, moistureThreshold);
      activatePump(true);
      pumpStartTime = currentMillis;
    } else if (moistureLevel >= moistureThreshold && pumpRunning) {
      logMessagef(LOG_LEVEL_INFO, "AUTO", "Moisture restored (%d%% >= %d%%). Deactivating pump.",
                  moistureLevel, moistureThreshold);
      activatePump(false);
    }
  }

  checkPumpTimeout();

  if (currentMillis - lastPublishTime > publishInterval) {
    publishStatus();
    lastPublishTime = currentMillis;
  }

  static unsigned long lastStatusLog = 0;
  if (currentMillis - lastStatusLog > 5000 && currentLogLevel >= LOG_LEVEL_DEBUG) {
    logMessagef(LOG_LEVEL_DEBUG, "SYSTEM", "Loop time: %ld ms, Moisture: %d%%",
                currentMillis - lastLoopTime, moistureLevel);
    lastStatusLog = currentMillis;
  }

  delay(10);
  lastLoopTime = currentMillis;
}

void connectToWiFi() {
  logMessagef(LOG_LEVEL_INFO, "WIFI", "Connecting to network: %s", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("┌────────────────────────────────────────────────────┐");
  Serial.println("│ WiFi Connection Status                             │");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (attempts % 20 == 0) {
      Serial.println("│                                                    │");
      Serial.print("│ Attempt: ");
      Serial.print(attempts);
      Serial.print(" ");
    }
    delay(100);
    Serial.print("▫");
    attempts++;

    if (attempts > 300) {
      Serial.println("\n│ Connection timeout. Retrying...                    │");
      WiFi.disconnect();
      delay(100);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      attempts = 0;
    }
  }

  Serial.println("\n└────────────────────────────────────────────────────┘");

  char ipBuffer[20];
  sprintf(ipBuffer, "%d.%d.%d.%d",
          WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);

  logMessage(LOG_LEVEL_INFO, "WIFI", "Connection established");
  logMessagef(LOG_LEVEL_INFO, "WIFI", "IP address: %s", ipBuffer);
  logMessagef(LOG_LEVEL_DEBUG, "WIFI", "Signal strength: %d dBm", WiFi.RSSI());
}

void connectToAWS() {
  logMessage(LOG_LEVEL_INFO, "MQTT", "Connecting to AWS IoT Core...");

  Serial.println("┌────────────────────────────────────────────────────┐");
  Serial.println("│ AWS IoT Connection                                 │");

  int attempts = 0;
  while (!mqttClient.connected()) {
    if (mqttClient.connect(CLIENT_ID)) {
      Serial.println("│ ✓ Connected successfully                           │");
      Serial.println("└────────────────────────────────────────────────────┘");

      logMessage(LOG_LEVEL_INFO, "MQTT", "Connected to AWS IoT Core");

      mqttClient.subscribe(CONTROL_TOPIC);
      logMessagef(LOG_LEVEL_INFO, "MQTT", "Subscribed to topic: %s", CONTROL_TOPIC);

      publishStatus();
    } else {
      int state = mqttClient.state();

      const char* errorMsg;
      switch (state) {
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

      if (attempts % 3 == 0) {
        Serial.print("│ ✗ Connection attempt ");
        Serial.print(attempts);
        Serial.print(": ");
        Serial.print(errorMsg);
        Serial.println("              │");
      }

      logMessagef(LOG_LEVEL_ERROR, "MQTT", "Connection failed: %s (code: %d)", errorMsg, state);

      attempts++;
      if (attempts % 3 == 0) {
        logMessagef(LOG_LEVEL_WARNING, "MQTT", "Multiple connection failures. Check credentials and network.");
      }

      delay(200);
    }
  }
}

void messageHandler(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';

  logMessagef(LOG_LEVEL_DEBUG, "MQTT", "Message received on topic: %s", topic);
  logMessagef(LOG_LEVEL_DEBUG, "MQTT", "Payload: %s", message);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    logMessagef(LOG_LEVEL_ERROR, "JSON", "Parsing failed: %s", error.c_str());
    return;
  }

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

      if (pumpRunning) {
        logMessage(LOG_LEVEL_INFO, "MODE", "Auto-disabling pump due to mode change");
        activatePump(false);
      }
    } else if (strcmp(mode, "manual") == 0) {
      operationMode = MANUAL_MODE;
      logMessage(LOG_LEVEL_INFO, "MODE", "Switched to MANUAL mode");

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
      logMessagef(LOG_LEVEL_INFO, "CONF", "Moisture threshold updated: %d%% → %d%%",
                  oldThreshold, moistureThreshold);
    } else {
      logMessagef(LOG_LEVEL_WARNING, "CONF", "Invalid threshold value received: %d (valid range: 0-100)",
                  newThreshold);
    }
  } else if (strcmp(command, "change_threshold") == 0) {
    int newThreshold = doc["value"];
    if (newThreshold >= 1 && newThreshold <= 20) {
      int oldThreshold = moistureChangeThreshold;
      moistureChangeThreshold = newThreshold;
      logMessagef(LOG_LEVEL_INFO, "CONF", "Moisture change threshold updated: %d%% → %d%%",
                  oldThreshold, moistureChangeThreshold);
    } else {
      logMessagef(LOG_LEVEL_WARNING, "CONF", "Invalid change threshold value received: %d (valid range: 1-20)",
                  newThreshold);
    }
  } else if (strcmp(command, "log_level") == 0) {
    int newLevel = doc["level"];
    if (newLevel >= LOG_LEVEL_ERROR && newLevel <= LOG_LEVEL_DEBUG) {
      int oldLevel = currentLogLevel;
      currentLogLevel = newLevel;

      const char* levelNames[] = { "ERROR", "WARNING", "INFO", "DEBUG" };
      logMessagef(LOG_LEVEL_INFO, "CONF", "Log level changed: %s → %s",
                  levelNames[oldLevel], levelNames[currentLogLevel]);
    }
  } else {
    logMessagef(LOG_LEVEL_WARNING, "CMD", "Unknown command received: %s", command);
  }

  publishStatus();
}

int readMoistureSensor() {
  int wetValue = 300;
  int dryValue = 1023;
  int rawValue = analogRead(MOISTURE_SENSOR_PIN);

  int moisturePercentage = map(rawValue, dryValue, wetValue, 0, 100);
  moisturePercentage = constrain(moisturePercentage, 0, 100);

  static unsigned long lastLogTime = 0;
  if (millis() - lastLogTime > 5000 && currentLogLevel >= LOG_LEVEL_DEBUG) {
    logMessagef(LOG_LEVEL_DEBUG, "SENSOR", "Raw moisture value: %d", rawValue);
    logMessagef(LOG_LEVEL_DEBUG, "SENSOR", "Moisture percentage: %d%%", moisturePercentage);
    lastLogTime = millis();
  }

  return moisturePercentage;
}

void activatePump(bool state) {
  if (state) {
    digitalWrite(PUMP_RELAY_PIN, LOW);
    pumpRunning = true;
    logMessagef(LOG_LEVEL_INFO, "PUMP", "Activated at %s", getTimestamp().c_str());
  } else {
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    pumpRunning = false;

    if (pumpStartTime > 0) {
      unsigned long pumpRuntime = millis() - pumpStartTime;
      logMessagef(LOG_LEVEL_INFO, "PUMP", "Deactivated. Run time: %.1f seconds", pumpRuntime / 1000.0);
      pumpStartTime = 0;
    } else {
      logMessage(LOG_LEVEL_INFO, "PUMP", "Deactivated");
    }
  }
}

void publishStatus() {
  int moistureLevel = readMoistureSensor();

  StaticJsonDocument<200> doc;

  doc["deviceId"] = CLIENT_ID;
  doc["moisture"] = moistureLevel;
  doc["timestamp"] = getTimestamp();
  doc["threshold"] = moistureThreshold;
  doc["pumpState"] = pumpRunning ? "on" : "off";
  doc["mode"] = operationMode ? "auto" : "manual";
  doc["uptime"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();
  doc["changeThreshold"] = moistureChangeThreshold;

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  logMessage(LOG_LEVEL_DEBUG, "MQTT", "Publishing status to AWS IoT");

  if (mqttClient.publish(STATUS_TOPIC, jsonBuffer)) {
    logMessage(LOG_LEVEL_INFO, "MQTT", "Status published successfully");
    logMessagef(LOG_LEVEL_DEBUG, "MQTT", "Status payload: %s", jsonBuffer);

    lastPublishTime = millis();
  } else {
    logMessage(LOG_LEVEL_ERROR, "MQTT", "Failed to publish status");
  }
}

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

void checkPumpTimeout() {
  if (pumpRunning && (millis() - pumpStartTime > maxPumpRuntime)) {
    logMessagef(LOG_LEVEL_WARNING, "SAFETY", "Pump timeout reached (%d seconds). Force stopping.",
                maxPumpRuntime / 1000);
    activatePump(false);
  }
}

void printProgressBar(int percent, int width) {
  int filledWidth = (width * percent) / 100;
  Serial.print("[");
  for (int i = 0; i < width; i++) {
    if (i < filledWidth) Serial.print("■");
    else Serial.print(" ");
  }
  Serial.print("] ");
  Serial.print(percent);
  Serial.println("%");
}

void printLogBox(int level, const char* module, const char* message) {
  const char* levelSymbols[] = { "✗", "⚠", "ℹ", "⚙" };
  const char* levelColors[] = { "\033[1;31m", "\033[1;33m", "\033[1;32m", "\033[1;34m" };
  const char* resetColor = "\033[0m";

  Serial.print("┌─");
  for (int i = 0; i < 62; i++) Serial.print("─");
  Serial.println("┐");

  Serial.print("│ ");
  Serial.print(levelColors[level]);
  Serial.print(levelSymbols[level]);
  Serial.print(" [");
  Serial.print(module);
  Serial.print("]");
  Serial.print(resetColor);

  int paddingLength = 57 - strlen(module);
  for (int i = 0; i < paddingLength; i++) Serial.print(" ");
  Serial.println("│");

  Serial.println("│                                                              │");

  int msgLen = strlen(message);
  int startPos = 0;
  while (startPos < msgLen) {
    Serial.print("│  ");
    int charsThisLine = 0;
    for (int i = 0; i < 60 && (startPos + i) < msgLen; i++) {
      Serial.print(message[startPos + i]);
      charsThisLine++;
    }

    for (int i = charsThisLine; i < 60; i++) Serial.print(" ");
    Serial.println("  │");

    startPos += 60;
  }

  Serial.println("│                                                              │");
  Serial.print("└─");
  for (int i = 0; i < 62; i++) Serial.print("─");
  Serial.println("┘");
}

void logMessage(int level, const char* module, const char* message) {
  if (level > currentLogLevel) return;

  const char* levelLabels[] = { "ERROR", "WARN ", "INFO ", "DEBUG" };
  const char* levelColors[] = { "\033[1;31m", "\033[1;33m", "\033[1;36m", "\033[1;35m" };
  const char* resetColor = "\033[0m";

  Serial.print(levelColors[level]);
  Serial.print("┌── ");
  Serial.print(levelLabels[level]);
  Serial.print(" ───");
  for (int i = 0; i < 53; i++) Serial.print("─");
  Serial.println("┐");

  Serial.print("│ [");
  Serial.print(getTimestamp());
  Serial.print("] [");
  Serial.print(module);
  Serial.print("] ");

  int lineLen = 14 + strlen(module) + strlen(message);
  if (lineLen <= 64) {
    Serial.print(message);
    for (int i = 0; i < (64 - lineLen); i++) Serial.print(" ");
    Serial.println("│");
  } else {
    int msgLen = strlen(message);
    int startPos = 0;
    int firstLineSpace = 64 - 14 - strlen(module);

    for (int i = 0; i < firstLineSpace && i < msgLen; i++) {
      Serial.print(message[i]);
    }
    Serial.println("│");

    startPos = firstLineSpace;
    while (startPos < msgLen) {
      Serial.print("│ ");
      int charsThisLine = 0;
      for (int i = 0; i < 61 && (startPos + i) < msgLen; i++) {
        Serial.print(message[startPos + i]);
        charsThisLine++;
      }

      for (int i = charsThisLine; i < 61; i++) Serial.print(" ");
      Serial.println("│");

      startPos += 61;
    }
  }

  Serial.print("└");
  for (int i = 0; i < 66; i++) Serial.print("─");
  Serial.println("┘");
  Serial.print(resetColor);
}

void logMessagef(int level, const char* module, const char* format, ...) {
  if (level > currentLogLevel) return;

  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  logMessage(level, module, buffer);
}
