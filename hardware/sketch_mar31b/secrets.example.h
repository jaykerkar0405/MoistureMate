// WiFi credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// AWS IoT Core configuration
const int AWS_PORT = 8883;
const char* CLIENT_ID = "ESP8266_Plant_Monitor";
const char* STATUS_TOPIC = "plant/plant_name/status";
const char* CONTROL_TOPIC = "plant/plant_name/control";
const char* AWS_ENDPOINT = "xxxxxxx-ats.iot.us-east-1.amazonaws.com";

// AWS root CA certificate
const char* AWS_CERT_CA = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

// Device certificate
const char* AWS_CERT_CRT = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

// Device private key
const char* AWS_CERT_PRIVATE = R"EOF(
-----BEGIN RSA PRIVATE KEY-----

-----END RSA PRIVATE KEY-----
)EOF";
