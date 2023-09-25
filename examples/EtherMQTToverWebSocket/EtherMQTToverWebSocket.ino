#include <Ethernet.h>
#include <WebSocketsClient.h>  // include before MQTTPubSubClient.h
#include <MQTTPubSubClient.h>

uint8_t mac[] = {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45};
const IPAddress ip(192, 168, 0, 201);

WebSocketsClient client;
MQTTPubSubClient mqtt;

void connect() {
connect_to_host:
    Serial.println("connecting to host...");
    client.disconnect();
    client.begin("test.mosquitto.org", 8080, "/", "mqtt");  // "mqtt" is required
    client.setReconnectInterval(2000);

    Serial.print("connecting to mqtt broker...");
    while (!mqtt.connect("arduino", "public", "public")) {
        Serial.print(".");
        delay(1000);
        if (!client.isConnected()) {
            Serial.println("WebSocketsClient disconnected");
            goto connect_to_host;
        }
    }
    Serial.println(" connected!");
}

void setup() {
    Serial.begin(115200);
    Ethernet.begin(mac, ip);

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
