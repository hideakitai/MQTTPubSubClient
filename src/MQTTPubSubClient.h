#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <Client.h>

extern "C" {
#include "MQTTPubSubClient/lwmqtt/lwmqtt.h"
}

#include "MQTTPubSubClient/util/ArxTypeTraits/ArxTypeTraits.h"
#include "MQTTPubSubClient/util/ArxContainer/ArxContainer.h"

#ifdef WEBSOCKETS_H_
#define MQTTPUBSUBCLIENT_USE_WEBSOCKETS
#endif

namespace arduino {
namespace mqtt {

#ifdef MQTTPUBSUBCLIENT_USE_WEBSOCKETS
    using ClientType = WebSocketsClient;
#else
    using ClientType = Client;
#endif

    template <size_t BUFFER_SIZE, size_t CALLBACK_SIZE = 16>
    class PubSubClient {
        // ---------- lwmqtt interface types ----------
        // these structs are saved to inside of lwmqtt and invoked as (void*)
        // and inside of our static methods, these void* are converted to this struct and used

        typedef uint32_t (*MQTTClientClockSource)();
        typedef struct {
            uint32_t start_ms;
            uint32_t timeout_ms;
            MQTTClientClockSource millis;
        } lwmqtt_arduino_timer_t;

        typedef struct {
            ClientType* client;
#ifdef MQTTPUBSUBCLIENT_USE_WEBSOCKETS
#if ARX_HAVE_LIBSTDCPLUSPLUS >= 201103L  // Have libstdc++11
            std::deque<uint8_t> buffer;
#else
            arx::deque<uint8_t, BUFFER_SIZE> buffer;
#endif
#endif
        } lwmqtt_arduino_network_t;

        typedef struct {
            PubSubClient<BUFFER_SIZE>* mqtt_client;
        } lwmqtt_arduino_client_callback_t;

        // ---------- interfaces between lwmqtt and library ----------

        uint8_t lwmqtt_read_buf[BUFFER_SIZE];
        uint8_t lwmqtt_write_buf[BUFFER_SIZE];
#ifdef MQTTPUBSUBCLIENT_USE_WEBSOCKETS
        lwmqtt_arduino_network_t lwmqtt_network {nullptr, {0}};
#else
        lwmqtt_arduino_network_t lwmqtt_network {nullptr};
#endif
        lwmqtt_arduino_timer_t lwmqtt_timer1 {0, 0, nullptr};
        lwmqtt_arduino_timer_t lwmqtt_timer2 {0, 0, nullptr};
        lwmqtt_client_t lwmqtt_client {lwmqtt_client_t()};
        lwmqtt_arduino_client_callback_t lwmqtt_callback;

        // ---------- PubSubClient members ----------

        using global_callback_t = std::function<void(const char*, const char*, const size_t)>;
        using topic_callback_t = std::function<void(const char*, const size_t)>;
#if ARX_HAVE_LIBSTDCPLUSPLUS >= 201103L  // Have libstdc++11
        using TopicCallbacks = std::map<String, topic_callback_t>;
#else
        using TopicCallbacks = arx::map<String, topic_callback_t, CALLBACK_SIZE>;
#endif

        // required variables
        ClientType* client {nullptr};
        lwmqtt_will_t* will {nullptr};
        String will_topic;
        String will_payload;
        global_callback_t global_callback;
        TopicCallbacks callbacks;
        uint32_t prev_keep_alive_ms {0};
        uint32_t keep_alive_interval_ms {10};

        // options
        uint32_t timeout_ms {1000};
        uint16_t keep_alive_timeout_sec {100};
        bool should_clean_session {true};

        // status
        bool is_connected {false};
        lwmqtt_return_code_t return_code {(lwmqtt_return_code_t)0};
        lwmqtt_err_t last_error {(lwmqtt_err_t)0};

    public:
        ~PubSubClient() {
            clearWill();
        }

        void begin(ClientType& client) {
            init(client);
        }

        bool connect(const String& client_id, const String& user = "", const String& pass = "") {
#ifdef MQTTPUBSUBCLIENT_USE_WEBSOCKETS
            while (!client->isConnected()) {
                client->loop();
                delay(10);
            }
#endif
            lwmqtt_options_t options = lwmqtt_default_options;
            options.keep_alive = keep_alive_timeout_sec;
            options.clean_session = should_clean_session;
            options.client_id = lwmqtt_string(client_id.c_str());
            if (user.length()) {
                options.username = lwmqtt_string(user.c_str());
                if (pass.length()) {
                    options.password = lwmqtt_string(pass.c_str());
                }
            }

            last_error = lwmqtt_connect(&lwmqtt_client, options, will, &return_code, timeout_ms);
            if (last_error != LWMQTT_SUCCESS) {
                close();
                return false;
            }
            is_connected = true;
            return true;
        }

        bool publish(const String& topic, const String& payload, const bool retained = false, int qos = 0) {
            return publish(topic.c_str(), (uint8_t*)payload.c_str(), payload.length(), retained, qos);
        }
        bool publish(const String& topic, uint8_t* payload, const size_t length, const bool retained = false, const uint8_t qos = 0) {
            if (!isConnected())
                return false;

            lwmqtt_message_t message = lwmqtt_default_message;
            message.payload = payload;
            message.payload_len = length;
            message.retained = retained;
            message.qos = lwmqtt_qos_t(qos);

            last_error = lwmqtt_publish(&lwmqtt_client, lwmqtt_string(topic.c_str()), message, timeout_ms);
            if (last_error != LWMQTT_SUCCESS) {
                close();
                return false;
            }
            return true;
        }

        void subscribe(const global_callback_t& cb) {
            global_callback = cb;
        }

        bool subscribe(const String& topic, const topic_callback_t& cb) {
            return subscribe(topic, 0, cb);
        }

        bool subscribe(const String& topic, const uint8_t qos, const topic_callback_t& cb) {
            if (!isConnected())
                return false;

            last_error = lwmqtt_subscribe_one(&lwmqtt_client, lwmqtt_string(topic.c_str()), (lwmqtt_qos_t)qos, timeout_ms);
            if (last_error != LWMQTT_SUCCESS) {
                close();
                return false;
            }
            callbacks[topic] = cb;
            return true;
        }

        bool unsubscribe(const String& topic) {
            if (!isConnected())
                return false;

            last_error = lwmqtt_unsubscribe_one(&lwmqtt_client, lwmqtt_string(topic.c_str()), timeout_ms);
            if (last_error != LWMQTT_SUCCESS) {
                close();
                return false;
            }
            callbacks.erase(topic);
            return true;
        }

        bool update() {
            if (!isConnected())
                return false;

#ifdef MQTTPUBSUBCLIENT_USE_WEBSOCKETS
            client->loop();
            // data is already received in onEvent callback
            auto available = lwmqtt_network.buffer.size();
#else
            auto available = (size_t)client->available();
#endif
            if (available > 0) {
                last_error = lwmqtt_yield(&lwmqtt_client, available, timeout_ms);
                if (last_error != LWMQTT_SUCCESS) {
                    close();
                    return false;
                }
            }

            uint32_t now_ms = millis();
            if (now_ms > prev_keep_alive_ms + keep_alive_interval_ms) {
                prev_keep_alive_ms = now_ms;
                last_error = lwmqtt_keep_alive(&lwmqtt_client, timeout_ms);
                if (last_error != LWMQTT_SUCCESS) {
                    close();
                    return false;
                }
            }
            return true;
        }

        void setClockSource(const MQTTClientClockSource& cb) {
            lwmqtt_timer1.millis = cb;
            lwmqtt_timer2.millis = cb;
        }

        void setWill(const String& topic, const String& payload = "", const bool retained = false, const uint8_t qos = 0) {
            if (topic.length() == 0)
                return;

            clearWill();

            will = (lwmqtt_will_t*)malloc(sizeof(lwmqtt_will_t));
            memset(will, 0, sizeof(lwmqtt_will_t));

            // save topic and payload to use them later
            will_topic = topic;
            will_payload = payload;

            will->topic = lwmqtt_string(will_topic.c_str());
            if (will_payload.length() > 0)
                will->payload = lwmqtt_string(will_payload.c_str());
            will->retained = retained;
            will->qos = (lwmqtt_qos_t)qos;
        }

        void clearWill() {
            if (will != nullptr) {
                free(will);
                will = nullptr;
            }
        }

        void setKeepAliveSendInterval(const uint32_t ms) {
            keep_alive_interval_ms = ms;
        }

        void setKeepAliveTimeout(const uint16_t sec) {
            keep_alive_timeout_sec = sec;
        }

        void setCleanSession(const bool b) {
            should_clean_session = b;
        }

        void setTimeout(const uint32_t ms) {
            timeout_ms = ms;
        }

        void setOptions(const uint16_t keep_alive_timeout_sec, const bool b_clean_session, const uint32_t timeout_ms) {
            setKeepAliveTimeout(keep_alive_timeout_sec);
            setCleanSession(b_clean_session);
            setTimeout(timeout_ms);
        }

        bool isConnected() const {
#ifdef MQTTPUBSUBCLIENT_USE_WEBSOCKETS
            return client->isConnected() && (is_connected);
#else
            return (client->connected() == 1) && (is_connected);
#endif
        }

        bool disconnect() {
            if (!isConnected())
                return false;

            last_error = lwmqtt_disconnect(&lwmqtt_client, timeout_ms);
            close();
            return last_error == LWMQTT_SUCCESS;
        }

        lwmqtt_err_t getLastError() const {
            return last_error;
        }
        lwmqtt_return_code_t getReturnCode() const {
            return return_code;
        }

    private:
        void init(ClientType& c) {
            client = &c;
            lwmqtt_network.client = &c;
            lwmqtt_callback.mqtt_client = this;
            lwmqtt_init(&lwmqtt_client, lwmqtt_write_buf, BUFFER_SIZE, lwmqtt_read_buf, BUFFER_SIZE);
            lwmqtt_set_timers(&lwmqtt_client, &lwmqtt_timer1, &lwmqtt_timer2, lwmqtt_arduino_timer_set, lwmqtt_arduino_timer_get);
            lwmqtt_set_network(&lwmqtt_client, &lwmqtt_network, lwmqtt_arduino_network_read, lwmqtt_arduino_network_write);
            lwmqtt_set_callback(&lwmqtt_client, (void*)&lwmqtt_callback, lwmqtt_arduino_mqtt_client_handler);
#ifdef MQTTPUBSUBCLIENT_USE_WEBSOCKETS
            c.onEvent([&](WStype_t type, uint8_t* payload, size_t length) {
                switch (type) {
                    case WStype_DISCONNECTED:
                        break;
                    case WStype_CONNECTED:
                        break;
                    case WStype_TEXT:
                    case WStype_BIN:
                        // copy to temprary buffer
                        for (size_t i = 0; i < length; ++i)
                            lwmqtt_network.buffer.emplace_back(payload[i]);
                        break;
                    case WStype_ERROR:
                    case WStype_FRAGMENT_TEXT_START:
                    case WStype_FRAGMENT_BIN_START:
                    case WStype_FRAGMENT:
                    case WStype_FRAGMENT_FIN:
                    case WStype_PING:
                    case WStype_PONG:
                        break;
                }
            });
#endif
        }

        void close() {
            is_connected = false;
#ifdef MQTTPUBSUBCLIENT_USE_WEBSOCKETS
            client->disconnect();
#else
            client->stop();
#endif
        }

        // ---------- lwmqtt callbacks ----------

        static void lwmqtt_arduino_timer_set(void* ref, uint32_t timeout_ms) {
            // cast timer reference
            auto t = (lwmqtt_arduino_timer_t*)ref;
            // set timeout
            t->timeout_ms = timeout_ms;
            // set start
            if (t->millis != nullptr) {
                t->start_ms = t->millis();
            } else {
                t->start_ms = millis();
            }
        }

        static int32_t lwmqtt_arduino_timer_get(void* ref) {
            // cast timer reference
            auto t = (lwmqtt_arduino_timer_t*)ref;
            // get now
            uint32_t now;
            if (t->millis != nullptr) {
                now = t->millis();
            } else {
                now = millis();
            }
            // get difference (account for roll-overs)
            uint32_t diff;
            if (now < t->start_ms) {
                diff = UINT32_MAX - t->start_ms + now;
            } else {
                diff = now - t->start_ms;
            }
            // return relative time
            if (diff > t->timeout_ms) {
                return -(diff - t->timeout_ms);
            } else {
                return t->timeout_ms - diff;
            }
        }

        static lwmqtt_err_t lwmqtt_arduino_network_read(void* ref, uint8_t* buffer, size_t len, size_t* read, uint32_t timeout_ms) {
            // cast network reference
            auto n = (lwmqtt_arduino_network_t*)ref;
            // set timeout
            uint32_t start_ms = millis();
            // reset counter
            *read = 0;
            // read until all bytes have been read or timeout has been reached
            while (len > 0 && (millis() < start_ms + timeout_ms)) {
#ifdef MQTTPUBSUBCLIENT_USE_WEBSOCKETS
                n->client->loop();
                // received data are already read to read_buf in onEvent callback
                int r = 0;
                if (n->buffer.size() >= len) {
                    for (size_t i = 0; i < len; ++i)
                        buffer[i] = n->buffer[i];
                    for (size_t i = 0; i < len; ++i)
                        n->buffer.pop_front();
                    r = len;
                }
#else
                // read from connection
                int r = n->client->read(buffer, len);
#endif
                // handle read data
                if (r > 0) {
                    buffer += r;
                    *read += r;
                    len -= r;
                    continue;
                }

                // wait/unblock for some time (RTOS based boards may otherwise fail since
                // the wifi task cannot provide the data)
                delay(0);
                // otherwise check status
#ifdef MQTTPUBSUBCLIENT_USE_WEBSOCKETS
                if (!n->client->isConnected()) {
#else
                if (!n->client->connected()) {
#endif
                    return LWMQTT_NETWORK_FAILED_READ;
                }
            }
            // check counter
            if (*read == 0) {
                return LWMQTT_NETWORK_TIMEOUT;
            }
            return LWMQTT_SUCCESS;
        }

        static lwmqtt_err_t lwmqtt_arduino_network_write(void* ref, uint8_t* buffer, size_t len, size_t* sent, uint32_t /*timeout_ms*/) {
            // cast network reference
            auto n = (lwmqtt_arduino_network_t*)ref;
            // write bytes
#ifdef MQTTPUBSUBCLIENT_USE_WEBSOCKETS
            bool b = n->client->sendBIN(buffer, len);
            *sent = b ? len : 0;
#else
            *sent = n->client->write(buffer, len);
#endif
            if (*sent <= 0) {
                return LWMQTT_NETWORK_FAILED_WRITE;
            }
            return LWMQTT_SUCCESS;
        }

        // this function will be called inside of lwmqtt when packet has come
        static void lwmqtt_arduino_mqtt_client_handler(lwmqtt_client_t* /*client*/, void* ref, lwmqtt_string_t topic, lwmqtt_message_t message) {
            // null terminate topic
            char terminated_topic[topic.len + 1];
            memcpy(terminated_topic, topic.data, topic.len);
            terminated_topic[topic.len] = '\0';

            // null terminate payload if available
            if (message.payload != nullptr) {
                message.payload[message.payload_len] = '\0';
            }

            auto cb = (lwmqtt_arduino_client_callback_t*)ref;
            if (cb->mqtt_client->global_callback) {
                cb->mqtt_client->global_callback(terminated_topic, (const char*)message.payload, message.payload_len);
            }
            for (const auto& c : cb->mqtt_client->callbacks) {
                if (c.first == terminated_topic) {
                    c.second((const char*)message.payload, message.payload_len);
                }
            }
        }
    };

}  // namespace mqtt
}  // namespace arduino

namespace MQTTPubSub = arduino::mqtt;
using MQTTPubSubClient = arduino::mqtt::PubSubClient<128>;

#endif
