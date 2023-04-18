#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>


#include "display.hpp"
#include "scale.hpp"

WiFiClient espClient;
PubSubClient client(espClient);

const char *ssid = "smartstuff24_IoT"; // Change this to your WiFi SSID
const char *password = "iamt0tally1nsecur3"; // Change this to your WiFi password
long lastReconnectAttempt = 0;


void setup() {
  Serial.begin(115200);
  while(!Serial){delay(100);}

  
  setupDisplay();
  setupScale();

  Serial.println();
  Serial.println("******************************************************");
  // Serial.print("Connecting to ");
  // Serial.println(ssid);

  // WiFi.begin(ssid, password);

  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }

  // Serial.println("");
  // Serial.println("WiFi connected");
  // Serial.println("IP address: ");
  // Serial.println(WiFi.localIP());
  // lastReconnectAttempt = 0;
  // client.setServer("192.168.1.201", 1883);
}

boolean reconnect() {
  if (client.connect("coffee-scale")) {
    // Once connected, publish an announcement...
    Serial.println("Connected to MQTT");
  }
  return client.connected();
}

void loop() {
  // if (!client.connected()) {
  //   long now = millis();
  //   if (now - lastReconnectAttempt > 5000) {
  //     lastReconnectAttempt = now;
  //     // Attempt to reconnect
  //     if (reconnect()) {
  //       lastReconnectAttempt = 0;
  //     }
  //   }
  // } else {
  //   client.loop();
  // }
  //rotary_loop();
  delay(1000);
}
