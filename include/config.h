// #define DEBUG_MSG(...) 
#define DEBUG_MSG(...) Serial.printf(__VA_ARGS__)

// Custom settings
char baseMQTT[] = "home/test";  // Base
char area[] = "freezer"; // area in base
char state[] = "state"; // Refer comments below


// WiFi
#define MAXWIFI_CONNECT_RETRYS 10 // How many times to try connecting to WiFi before giving up
#define WIFI_RECONNECT_DELAY 300   // Seconds delay before retrying WiFi connection

#define WIFI_CHANNEL 1   // 1, 5, 9, 13 etc
#define STATION_SSID  "Atkinson"
#define STATION_PASSWORD "ferret11"

// MQTT service
#define MQTT_USER "atkmqtt"
// #define MQTT_USER "bbsmqtt"
#define MQTT_PASSWORD "ferret11"
#define MQTT_SERVER "192.168.1.150"
// #define MQTT_SERVER "192.168.22.136"

#define MAX_RETRY_BEFORE_REBOOT 5 // Number of connection retries before rebooting the system
#define MQTT_RECONNECT_RETRY_DELAY 30000   // MS before retrying connection to Home Assistant MQTT service
#define SCREEN_UPDATE_DELAY 10000
// LED flash speed - signal status of controller
#define CONNECTED_BLINK_PERIOD 10000 // milliseconds until cycle repeat
#define CONNECTED_BLINK_DURATION 100  // milliseconds LED is on for
#define WAITING_BLINK_PERIOD 1000
#define WAITING_BLINK_DURATION 100
#define MQTT_SENSORS_UPDATE_DELAY 120000  // milliseconds between sensor data updates via MQTT