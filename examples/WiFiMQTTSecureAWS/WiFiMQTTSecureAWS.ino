// this example is based on
// https://savjee.be/2019/07/connect-esp32-to-aws-iot-with-arduino-code/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTTPubSubClient.h>
#include <ArduinoJson.h>

// The MQTTT endpoint for the device (unique for each AWS account but shared amongst devices within the account)
const char* AWS_IOT_ENDPOINT = "your-endpoint.iot.ap-somewhre.amazonaws.com";

// The name of the device MUST match up with the name defined in the AWS console
const char* DEVICE_NAME = "your-device-name";

// Amazon's root CA. This should be the same for everyone.
const char AWS_CERT_CA[] =
    "-----BEGIN CERTIFICATE-----\n"
    "-----END CERTIFICATE-----\n";

// The private key for your device
const char AWS_CERT_PRIVATE[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "-----END RSA PRIVATE KEY-----\n";

// The certificate for your device
const char AWS_CERT_CRT[] =
    "-----BEGIN CERTIFICATE-----\n"
    "-----END CERTIFICATE-----\n";

const char* ssid = "your-ssid";
const char* pass = "your-password";

WiFiClientSecure client;
MQTTPubSubClient mqtt;

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, pass);

    Serial.print("connecting to wifi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println(" connected!");

    Serial.print("connecting to host...");
    // connect to aws endpoint with certificates and keys
    client.setCACert(AWS_CERT_CA);
    client.setCertificate(AWS_CERT_CRT);
    client.setPrivateKey(AWS_CERT_PRIVATE);
    while (!client.connect(AWS_IOT_ENDPOINT, 8883)) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println(" connected!");

    // initialize mqtt client
    mqtt.begin(client);

    Serial.print("connecting to aws mqtt broker...");
    while (!mqtt.connect(DEVICE_NAME)) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println(" connected!");

    // subscribe callback which is called when every packet has come
    mqtt.subscribe([](const String& topic, const String& payload, const size_t size) {
        Serial.println("mqtt received: " + topic + " - " + payload);
    });

    // subscribe topic and callback which is called when /hello has come
    mqtt.subscribe("/hello", [](const String& payload, const size_t size) {
        Serial.print("/hello ");
        Serial.println(payload);
    });
}

void loop() {
    mqtt.update();

    static uint32_t prev_ms = millis();
    if (millis() > prev_ms + 5000) {
        prev_ms = millis();

        StaticJsonDocument<128> doc;
        doc["from"] = "thing";
        char buffer[128];
        serializeJson(doc, buffer);

        mqtt.publish("/hello", buffer);
    }
}
