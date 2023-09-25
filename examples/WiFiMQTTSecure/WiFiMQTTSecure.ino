#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTTPubSubClient.h>

const char* ssid = "your-ssid";
const char* pass = "your-password";

WiFiClientSecure client;
MQTTPubSubClient mqtt;

void connect() {
connect_to_wifi:
    Serial.print("connecting to wifi...");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println(" connected!");

connect_to_host:
    Serial.print("connecting to host...");
    client.stop();
    client.setInsecure();  // skip verification
    while (!client.connect("public.cloud.shiftr.io", 8883)) {
        Serial.print(".");
        delay(1000);
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected");
            goto connect_to_wifi;
        }
    }
    Serial.println(" connected!");

    Serial.print("connecting to mqtt broker...");
    mqtt.disconnect();
    while (!mqtt.connect("arduino", "public", "public")) {
        Serial.print(".");
        delay(1000);
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected");
            goto connect_to_wifi;
        }
        if (client.connected() != 1) {
            Serial.println("WiFiClient disconnected");
            goto connect_to_host;
        }
    }
    Serial.println(" connected!");
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, pass);

    // initialize mqtt client
    mqtt.begin(client);

    // subscribe callback which is called when every packet has come
    mqtt.subscribe([](const String& topic, const String& payload, const size_t size) {
        Serial.println("mqtt received: " + topic + " - " + payload);
    });

    // subscribe topic and callback which is called when /hello has come
    mqtt.subscribe("/hello", [](const String& payload, const size_t size) {
        Serial.print("/hello ");
        Serial.println(payload);
    });

    connect();
}

void loop() {
    mqtt.update();  // should be called

    if (!mqtt.isConnected()) {
        connect();
    }

    // publish message
    static uint32_t prev_ms = millis();
    if (millis() > prev_ms + 1000) {
        prev_ms = millis();
        mqtt.publish("/hello", "world");
    }
}
