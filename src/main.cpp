#include <Arduino.h>

// Custom configuration
#include "config.h"

// Include the correct display library
// For a connection via I2C using Wire include
//#include // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <TaskScheduler.h>
#include <ArduinoJson.h>

/********************************************************************/
// First we include the libraries
#include <OneWire.h> 
#include <DallasTemperature.h>
/********************************************************************/
// Data wire is plugged into pin 2 on the Arduino 
#define ONE_WIRE_BUS 2 
/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices  
// (not just Maxim/Dallas temperature ICs) 
OneWire oneWire(ONE_WIRE_BUS); 
/********************************************************************/
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

#define BLINK_LED 16 // LED_BUILTIN //D4    // LED pin
#define CONNECTED_BLINK_PERIOD 1000 // milliseconds until cycle repeat
#define CONNECTED_BLINK_DURATION 100  // milliseconds LED is on for

void ledOffCallback();
void ledOnCallback();
bool setupWiFiandMQTT();
bool setup_wifi();
void callback(char* topic, byte* payload, unsigned int length);
void defineMQTTnodes();
void reconnect();
void publishData(float p_temperature, float p_humidity, int p_moisture, float p_volts);
void updateDisplayData(float temp);
void showMessage(String message, int position);

void ICACHE_RAM_ATTR interrupt0();
void ICACHE_RAM_ATTR interrupt12();
void ICACHE_RAM_ATTR interrupt13();
void ICACHE_RAM_ATTR interrupt14();

ADC_MODE(ADC_VCC); // read supply voltage

bool displayOn = true; // Dislay info initially. Will switch off at next temp update

void interrupt0() // Right
{
  DEBUG_MSG("Right");
}
 
void interrupt12() // Down
{
 DEBUG_MSG("Down");
}
 
void interrupt13() // Up
{
 DEBUG_MSG("Up");
}
 
void interrupt14() // Push
{
 DEBUG_MSG("Push");
 displayOn = !displayOn;
}

// Sensor data
float sTemp = 0.0; 
float sHumidity = 0.0;
int sMoiture = 0;
int sVolts = 0;

// Scheduler
Scheduler userScheduler; // to control your personal task
unsigned long lastReconnectMillis = 0;
unsigned long lastSensorUpdateMillis = 0;

// Initialize the OLED display using Wire library
SSD1306 display(0x3c, 5, 4);

int counter = 1;

int blueLed = LED_BUILTIN;          // Blue LED is on GPIO 2 (LED_BUILTIN)
int greenLed = 16;                  // Green LED is on GPIO 16
 
int analogPin = A0;                 // Analog input is A0
int analogValue = 0;

WiFiClient nodeClient;
PubSubClient mqttClient(nodeClient);

bool wifiConnected = false;
unsigned long retryWiFiConnectDelay = 0;
unsigned long lastScreenUpdate = 0;
int connectionRetryCount = 0;       // How many times has the system tried to reconnect

Task blinkStatusTask;
bool onFlag = true; // Status LED
int BLINK_PERIOD = CONNECTED_BLINK_PERIOD;
int BLINK_DURATION = CONNECTED_BLINK_DURATION;

char nodeLocation[40];
char nodeSensors[40];
bool startupInitial = true;   // Used to flag startup to trigger initial actions following system start

void setup() {
  Serial.begin(115200);

  // Initialising the UI will init the display too.
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  showMessage("Starting...",10);
  display.display();
  sensors.begin(); 
  wifiConnected = setupWiFiandMQTT();
  if (wifiConnected) {
    showMessage("WiFi connected",20);
    display.display();
  }

  defineMQTTnodes();

  pinMode(0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(0), interrupt0, HIGH);    // Right
  pinMode(12, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(12), interrupt12, HIGH);  // Down
  pinMode(13, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(13), interrupt13, HIGH);  // Up
  pinMode(14, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(14), interrupt14, HIGH);  // Push
}

void drawProgressBarDemo() {
  int progress = (counter / 5) % 100;
  // draw the progress bar
  display.drawProgressBar(0, 32, 120, 10, progress);
  // draw the percentage as String
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, String(progress) + "%");
}

void showTemperature(float readValue, int position) {
  String myString = "Temp = ";     // empty string
  myString.concat(readValue);
  display.setFont(ArialMT_Plain_10);
  // The coordinates define the left starting point of the text
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, position, myString);
}

void showMessage(String message, int position) {
  display.setFont(ArialMT_Plain_10);
  // The coordinates define the left starting point of the text
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, position, message);
}

void loop() {
  userScheduler.execute();

  // clear the display
  // display.clear();
  // // draw the current demo method
  // // // demos[demoMode]();

  // display.setTextAlignment(TEXT_ALIGN_RIGHT);
  // display.drawString(10, 128, String(millis()));
  // // write the buffer to the display
  // // display.display();


  if (connectionRetryCount > MAX_RETRY_BEFORE_REBOOT) {
    DEBUG_MSG("Max WiFi connect retry exceeded. Restarting ...\n");
    /// Unable to reconnect MQTT so resort to this.
    ESP.restart();
  }
  if (!mqttClient.connected()) {
    if ((millis() - lastReconnectMillis > MQTT_RECONNECT_RETRY_DELAY))
    {
      if (wifiConnected) { 
        connectionRetryCount++; 
        reconnect();
      }
      lastReconnectMillis = millis();
    }
    BLINK_PERIOD = WAITING_BLINK_PERIOD;
    BLINK_DURATION = WAITING_BLINK_DURATION;
  }
  else {
    BLINK_PERIOD = CONNECTED_BLINK_PERIOD;
    BLINK_DURATION = CONNECTED_BLINK_DURATION;
  
    if ((millis() - lastSensorUpdateMillis > MQTT_SENSORS_UPDATE_DELAY)) {
      sensors.requestTemperatures(); // Send the command to get temperature readings 
      sTemp = sensors.getTempCByIndex(0);
      DEBUG_MSG("Publishing data...");
      publishData(sTemp,sHumidity,sMoiture,(float)sVolts/1000);
      lastSensorUpdateMillis = millis();
    }
    updateDisplayData(sTemp);
  }

  if (wifiConnected) {
    mqttClient.loop();
  }
  else if (millis() - retryWiFiConnectDelay > (WIFI_RECONNECT_DELAY * 1000)) {
    wifiConnected = setupWiFiandMQTT();
    retryWiFiConnectDelay = millis();
    connectionRetryCount++; 
  }

  // digitalWrite(BLINK_LED, onFlag);  
}


void  ledOnCallback() {
 onFlag = false;
 blinkStatusTask.setCallback(&ledOffCallback);
 blinkStatusTask.setInterval(BLINK_PERIOD);
}

void ledOffCallback() {
  onFlag = true;
  blinkStatusTask.setCallback(&ledOnCallback);
  blinkStatusTask.setInterval(BLINK_DURATION);
}

void updateDisplayData(float temp) {
  if ((millis() - lastScreenUpdate > SCREEN_UPDATE_DELAY) && displayOn) {
    DEBUG_MSG("Update screen...");
    display.displayOn();
    display.clear();
    showTemperature(temp, 30);
    showMessage("MQTT connected", 10);
    sVolts = ESP.getVcc();
    DEBUG_MSG("Volts = %i", sVolts);
    showMessage("Volts = "+String((float)sVolts/1000),40);
    display.display();
    lastScreenUpdate = millis();
  }
  else if (!displayOn) {
    display.displayOff();
    lastScreenUpdate = 0;
  }
}

bool setup_wifi() {
  int _retryCount = 0;

  // We start by connecting to a WiFi network
  // sprintf(dispMsg, "\n\nConnecting to %s\n",STATION_SSID);  
  DEBUG_MSG("Connecting to %s\n",STATION_SSID);
  // int8_t _channel = WIFI_CHANNEL;
  WiFi.mode(WIFI_STA);
  // WiFi.channel(_channel);
  WiFi.begin(STATION_SSID, STATION_PASSWORD);

  while ((WiFi.status() != WL_CONNECTED) && _retryCount < MAXWIFI_CONNECT_RETRYS) {
    delay(500);
    DEBUG_MSG(".");
    _retryCount++;
  }
  wifi_set_channel(WIFI_CHANNEL);
  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_MSG("WiFi connected failed!\n");
    return false;
  }
  else {
    DEBUG_MSG("\nWiFi connected\nIP address %s\nChannel %i\n",WiFi.localIP().toString().c_str(), WiFi.channel());
    return true;
  }
  
}

bool setupWiFiandMQTT() {
  DEBUG_MSG("WiFi and MQTT setup. ");
  if (setup_wifi()) { 
    DEBUG_MSG("WiFi setup. Now MQTT...\n");
    mqttClient.setServer(MQTT_SERVER, 1883);
    mqttClient.setCallback(callback);
    return true;
  }
  DEBUG_MSG("WiFi setup failed\n");
  return false;
}

void callback(char* topic, byte* payload, unsigned int length) {
  String strTopic;
  payload[length] = '\0';
  strTopic = String((char*)topic);
  DEBUG_MSG("Callback: Payload = %s topic =  %s\n", payload, topic);
}

void defineMQTTnodes() {
  // Node location
  strcpy(nodeLocation,baseMQTT);
  strcat(nodeLocation,"/");
  strcat(nodeLocation,area);
  strcpy(nodeSensors,nodeLocation);
  strcat(nodeSensors,"/sensors");
}

void reconnect() {
  char _subscribe[40];
  strcpy(_subscribe, nodeLocation);
  strcat(_subscribe,"/#");
  DEBUG_MSG("Attempting MQTT connection...\n");
  if (mqttClient.connect(area, MQTT_USER, MQTT_PASSWORD)) {
    DEBUG_MSG("connected\n");
    // Once connected, subscribe 
    mqttClient.subscribe(_subscribe);
    // and publish an announcement...??
  } 
}

void publishData(float p_temperature, float p_humidity, int p_moisture, float p_volts) {  //, float p_airquality) {
    // create a JSON object
    StaticJsonDocument<200> jsonBuffer;
    jsonBuffer["temperature"] = (String)p_temperature;
    jsonBuffer["humidity"] = (String)p_humidity;
    jsonBuffer["moisture"] = (String)p_moisture;
    jsonBuffer["voltage"] = (String)p_volts;
    /*
    {
    "temperature": "23.20" ,
    "humidity": "43.70"
    }
    */
    char data[200];
    serializeJson(jsonBuffer,data);
    DEBUG_MSG("Publish data:\n");
    DEBUG_MSG(data);
    if (wifiConnected) { 
      mqttClient.publish(nodeSensors, data, true);
      yield();
    }
}

