#pragma once

#include <mqtt_client.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <set>
#include <string>

struct MqttMessage {
  std::string topic;
  std::string payload;
};

// Thin wrapper around ESP-IDF's esp-mqtt client. esp-mqtt runs in its own
// task; received messages are reassembled and queued here for the main loop.
class Mqtt {
 public:
  struct Config {
    const char* host;
    uint16_t port;
    const char* user;
    const char* password;
    const char* clientId;
    const char* willTopic;  // retained "OFF" if the sign drops off the network
  };

  ~Mqtt() { stop(); }

  bool start(const Config& config);
  void stop();
  bool connected() const { return connected_; }
  // Increments on every (re)connection; with a clean session each one needs
  // its subscriptions made again.
  uint32_t connectCount() const { return connectCount_; }

  // QoS 1 publishes are tracked until the broker acknowledges them.
  bool publish(const std::string& topic, const std::string& payload, bool retain, int qos = 0);
  bool subscribe(const std::string& filter, int qos = 1);

  bool poll(MqttMessage& out);  // next received message, if any
  size_t unacked() const;       // QoS 1 publishes still waiting for PUBACK

 private:
  static void onEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
  void handle(esp_mqtt_event_handle_t event);

  esp_mqtt_client_handle_t client_ = nullptr;
  std::atomic<bool> connected_{false};
  std::atomic<uint32_t> connectCount_{0};
  mutable std::mutex mutex_;
  std::deque<MqttMessage> inbox_;
  std::set<int> unacked_;
  std::set<int> ackedEarly_;  // PUBACKs that raced ahead of publish() returning
  MqttMessage partial_;       // large messages arrive in pieces
};
