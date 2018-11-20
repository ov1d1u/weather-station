# weather-station
Fully-autonomous weather station with Wemos Micro

# Setup

Create a file `data/config.json` with the following content:

```json
{
  "wifi_name": "Your WiFi network name",
  "wifi_pass": "Your WiFi password",
  "mqtt_server": "IP_OF_MQTT_SERVER",
  "mqtt_port": 1883
}
```

Adjust the code to suit your needs and write the sketch to a ESP8266-compatible board.
