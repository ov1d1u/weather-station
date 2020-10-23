// Arduino Pro or Pro Mini
// ATmega328P 3.3V 8MHz

#include <RF24.h>
#include <DHT.h>
#include <LiFuelGauge.h>
#include <LowPower.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <limits.h>
#include "DataPacket.pb.h"

#define TEMP_CHANNEL "WHTM"
#define HUM_CHANNEL "WHHM"
#define BATT_CHANNEL "WHBT"
#define SLEEP_DURATION 600 // 600s = 30 mins
uint8_t radioAddress[9] = "Gateway1";

RF24 radio(8, 10);
LiFuelGauge gauge(MAX17043);
DHT dht(4, DHT22);

void(*resetFunc) (void) = 0; //declare reset function @ address 0

void setup() {
  Serial.begin(9600);
  pinMode(2, OUTPUT);
  pinMode(6, OUTPUT);
  Serial.println("Done.");
}

void loop() {
  gauge.wake();
  gauge.quickStart();
  
  if (!wakeUp()) {
    Serial.println("Failed to wake up, aborting.");
    goToSleep();
    return;
  }
  delay(2000);

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
    Serial.print(" BATT: ");
    Serial.print(battery);
    Serial.println("");

    Serial.println("Publishing value over the air");
    // Temperature
    broadcastData(TEMP_CHANNEL, temperature);
    // Humidity
    broadcastData(HUM_CHANNEL, humidity);
    // Battery
    if (battery > 0.0) {  // Check we still have a battery, lol
      broadcastData(BATT_CHANNEL, battery);
    }
  }

  goToSleep();
}

bool wakeUp() {
  digitalWrite(2, HIGH);
  digitalWrite(6, HIGH);
  
  dht.begin();
  Serial.println("Initializing radio...");
  if (!radio.begin()) {
    Serial.println("FAIL");
    return false;
  }
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(radioAddress);
  return true;
}

void goToSleep() {
  Serial.println("All tasks done, going to sleep...");
  digitalWrite(2, LOW);
  digitalWrite(6, LOW);
  delay(200);

  for (unsigned int sleepCounter = SLEEP_DURATION/8; sleepCounter>0; sleepCounter--) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}

void broadcastData(char *channel, float value) {
  Serial.print("Sending "); Serial.print(DataPacket_size); Serial.println(" bytes to gateway...");
  uint8_t buf[DataPacket_size];
  uint8_t len = sizeof(buf);
  DataPacket packet = DataPacket_init_zero;
  packet.packet_type = DataPacket_PacketType_MEASUREMENT;
  strncpy(packet.identifier, channel, sizeof(packet.identifier));
  packet.value = value;
  pb_ostream_t ostream = pb_ostream_from_buffer(buf, DataPacket_size);
  if (pb_encode(&ostream, DataPacket_fields, &packet)) {
    radio.writeBlocking(&buf, ostream.bytes_written, 1000);
    radio.txStandBy(2000);
  }

  delay(500);
}
