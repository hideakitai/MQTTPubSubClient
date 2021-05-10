# MQTTPubSubClient

MQTT and MQTT over WebSoket Client for Arduino


## Features

- MQTT 3.1.1 based on [lwmqtt](https://github.com/256dpi/lwmqtt) ([arduino-mqtt](https://github.com/256dpi/arduino-mqtt))
    - publish and subscribe message
    - wildcard support for topic
    - qos 0/1/2
    - retain
    - will
    - keep alive (interval and timeout)
    - clean session
- MQTT over WebSocket by using with [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) library
- Multiple callback per topic (no need to write `if`-`else` in callback)
- Various boards support which has Arduino's `Client` class

## Usage

### MQTT

```C++
#include <WiFi.h>
// or
// #include <Ethernet.h>
#include <MQTTPubSubClient.h>

WiFiClient client;
// or
// EthernetClient client;
MQTTPubSubClient mqtt;

void setup() {
    // start your network
    WiFi.begin("your-ssid", "your-password");
    // connect to host
    client.connect("public.cloud.shiftr.io", 1883);
    // initialize mqtt client
    mqtt.begin(client);
    // connect to mqtt broker
    mqtt.connect("arduino", "public", "public");

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
    // should be called to trigger callbacks
    mqtt.update();
    // publish message
    mqtt.publish("/hello", "world");
    delay(1000);
}
```

### MQTT over WebSocket

Just replace `WiFiClient` or `EthernetClient` to `WebSocketsClient`.

```C++
#include <WiFi.h>
#include <WebSocketsClient.h>  // include before MQTTPubSubClient.h
#include <MQTTPubSubClient.h>

WebSocketsClient client;
MQTTPubSubClient mqtt;

void setup() {
    // start your network
    WiFi.begin("your-ssid", "your-password");
    // connect to host with MQTT over WebSocket securely
    client.beginSSL("public.cloud.shiftr.io", 443);
    client.setReconnectInterval(2000);
    // initialize mqtt client
    mqtt.begin(client);
    // connect to mqtt broker
    mqtt.connect("arduino", "public", "public");

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
    // should be called to trigger callbacks
    mqtt.update();
    // publish message
    mqtt.publish("/hello", "world");
    delay(1000);
}
```

### MQTT with Secure Connection (e.g. AWS IoT Platform)

If your board supports secure connection with certificates, it is also supprted same as above. Please see [WiFiMQTTSecureAWS](https://github.com/hideakitai/MQTTPubSubClient/examples/WiFiMQTTSecureAWS) example for the detail.

```C++
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTTPubSubClient.h>

const char AWS_CERT_CA[] =
    "-----BEGIN CERTIFICATE-----\n"
    "-----END CERTIFICATE-----\n";
const char AWS_CERT_PRIVATE[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "-----END RSA PRIVATE KEY-----\n";
const char AWS_CERT_CRT[] =
    "-----BEGIN CERTIFICATE-----\n"
    "-----END CERTIFICATE-----\n";

WiFiClientSecure client;
MQTTPubSubClient mqtt;

void setup() {
    // start your network
    WiFi.begin("your-ssid", "your-password");
    // connect to aws endpoint with certificates and keys
    client.setCACert(AWS_CERT_CA);
    client.setCertificate(AWS_CERT_CRT);
    client.setPrivateKey(AWS_CERT_PRIVATE);
    client.connect("your-endpoint.iot.ap-somewhre.amazonaws.com", 8883);
    // initialize mqtt client
    mqtt.begin(client);
    // connect to mqtt broker
    mqtt.connect("your-device-name");

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
    // should be called to trigger callbacks
    mqtt.update();
    // publish message
    mqtt.publish("/hello", "aws");
    delay(1000);
}
```


### Callback Signature

The following signature are available for callbacks.

```C++
// callback for every packet
void callback(const String& topic, const String& payload, const size_t size);
void callback(const String& topic, const char* payload, const size_t size);
void callback(const char* topic, const String& payload, const size_t size);
void callback(const const char* topic, const char* payload, const size_t size);

// callback for each topic
void callback(const String& payload, const size_t size);
void callback(const char* payload, const size_t size);
```


### Send / Receive Buffer Size Management

The default send / receive buffer size is 128 and defined as below.

```C++
using MQTTPubSubClient = MQTTPubSub::PubSubClient<128>;
```

If you want to change the buffer, please create instance like this.

```C++
MQTTPubSub::PubSubClient<256> mqtt;
```


## APIs

```C++
void begin(ClientType& client);
bool connect(const String& client_id, const String& user = "", const String& pass = "");
bool disconnect();
bool update();

bool publish(const String& topic, const String& payload, const bool retained = false, int qos = 0);
bool publish(const String& topic, uint8_t* payload, const size_t length, const bool retained = false, const uint8_t qos = 0);

void subscribe(const global_callback_t& cb);
bool subscribe(const String& topic, const topic_callback_t& cb);
bool subscribe(const String& topic, const uint8_t qos, const topic_callback_t& cb);
bool unsubscribe(const String& topic);

void setClockSource(const MQTTClientClockSource& cb);
void setWill(const String& topic, const String& payload = "", const bool retained = false, const uint8_t qos = 0);
void clearWill();
void setKeepAliveSendInterval(const uint32_t ms);
void setKeepAliveTimeout(const uint16_t sec);
void setCleanSession(const bool b);
void setTimeout(const uint32_t ms);
void setOptions(const uint16_t keep_alive_timeout_sec, const bool b_clean_session, const uint32_t timeout_ms);

bool isConnected() const;
lwmqtt_err_t getLastError() const;
lwmqtt_return_code_t getReturnCode() const;
```


## Embedded Libraries

- [ArxTypeTraits v0.2.1](https://github.com/hideakitai/ArxTypeTraits)
- [ArxContainer v0.3.14](https://github.com/hideakitai/ArxContainer)


## License

MIT
