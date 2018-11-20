#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <FS.h>
#include <DHT.h>
#include <LiFuelGauge.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const long MAX_LOOP_TIME_MS = 60000;

// Configuration
String WIFI_SSID;
String WIFI_PASSWORD;
String mqtt_server;
int mqtt_port = 0;

LiFuelGauge gauge(MAX17043);
DHT dht(D4, DHT22);
WiFiClient espClient;
PubSubClient mqttClient;
Ticker sleepTicker;

void setup() {
  // put your setup code here, to run once:
  sleepTicker.once_ms(MAX_LOOP_TIME_MS, &goToSleep);  // 60 sec timeout
  
  Serial.begin(9600);
  gauge.wake();
  dht.begin();
  delay(2000);
  
  if (!loadConfiguration()) {
    Serial.println("Failed to load config, aborting.");
    goToSleep();
    return;
  }

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  // Connect to WiFi, if needed
  if (WiFi.SSID() != WIFI_SSID) {
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
    WiFi.persistent(true);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
  }

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //espClient.setSync(true);
  mqttClient.setClient(espClient);
  mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
  Serial.print("Attempting MQTT connection to "); Serial.print(mqtt_server); Serial.print(":"); Serial.print(mqtt_port), Serial.print(" ");
  while (!mqttClient.connected()) {
    // Attempt to connect
    if (mqttClient.connect("Meteo")) {
      Serial.println(" connected");
    } else {
      Serial.print(".");
      delay(500);
    }
  }

}

void loop() {
  // put your main code here, to run repeatedly:
  mqttClient.loop();
  gauge.quickStart();

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  float realFeel = dht.computeHeatIndex(temperature, humidity, false);
  float battery = gauge.getSOC();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read sensor data.");
  } else {
    Serial.println("Read values:");
    Serial.print("TEMP: ");
    Serial.print(temperature);
    Serial.print(" HUM: ");
    Serial.print(humidity);
    if (battery > 0.0) {
      Serial.print(" BATT: ");
      Serial.print(battery);
    }
    Serial.println("");
  
    Serial.println("Publishing values over MQTT");
    mqttClient.publish("meteo/humidity", String(humidity, 1).c_str(), true);
    mqttClient.publish("meteo/temperature", String(temperature, 1).c_str(), true);
    mqttClient.publish("meteo/real_feel", String(realFeel, 1).c_str(), true);
    if (battery > 0.0) {  // Check we still have a battery, lol
      mqttClient.publish("meteo/battery", String(battery, 1).c_str(), true);
    }
    
    Serial.println("Done, preparing to sleep");
  }
  
  // Disconnect from the MQTT server and WiFi
  mqttClient.disconnect();
  delay(5000);
  WiFi.disconnect();
  while (WiFi.status() == WL_CONNECTED) { delay(500); }
  goToSleep();
}

bool loadConfiguration() {
  Serial.println("Reading config data...");
  SPIFFS.begin();
  // Do some checks...
  if (!SPIFFS.exists("/config.json")) {
    Serial.println("CONFIGURATION FILE DOES NOT EXIST, ABORTING.");
    SPIFFS.end();
    return false;
  }
  
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("FAILED TO OPEN CONFIGURATION FILE, ABORTING.");
    SPIFFS.end();
    return false;
  }

  // Read file's contents
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  // Parse file
  DynamicJsonDocument doc;
  DeserializationError jsonError = deserializeJson(doc, buf.get());
  if (jsonError) {
    Serial.println("FAILED TO PARSE CONFIGURATION FILE, ABORTING.");
    SPIFFS.end();
    return false;
  }

  // Load config values from files
  JsonObject json = doc.as<JsonObject>();
  WIFI_SSID = json["wifi_name"].as<String>();
  WIFI_PASSWORD = json["wifi_pass"].as<String>();
  mqtt_server = json["mqtt_server"].as<String>();
  mqtt_port = json["mqtt_port"];
  SPIFFS.end();

  return true;
}

void goToSleep() {
  if (millis() >= MAX_LOOP_TIME_MS) {
    Serial.println("Timeout occured, aborting everything and going to sleep.");
    WiFi.disconnect();
  }
  Serial.println("Going to sleep, see you in 15m.");
  sleepTicker.detach();
  gauge.sleep();
  ESP.deepSleep(900e6, WAKE_RF_DEFAULT);
  delay(5000);
}
