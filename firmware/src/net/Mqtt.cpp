#include "net/Mqtt.h"

namespace {
constexpr size_t kInboxLimit = 64;
}

bool Mqtt::start(const Config& config) {
  stop();
  esp_mqtt_client_config_t cfg = {};
  cfg.broker.address.hostname = config.host;
  cfg.broker.address.port = config.port;
  cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
  cfg.credentials.client_id = config.clientId;
  if (config.user != nullptr && config.user[0] != '\0') {
    cfg.credentials.username = config.user;
    cfg.credentials.authentication.password = config.password;
  }
  cfg.session.keepalive = 30;
  cfg.session.last_will.topic = config.willTopic;
  cfg.session.last_will.msg = "OFF";
  cfg.session.last_will.msg_len = 3;
  cfg.session.last_will.qos = 1;
  cfg.session.last_will.retain = 1;
  cfg.network.timeout_ms = 5000;
  cfg.network.reconnect_timeout_ms = 3000;
  cfg.buffer.size = 2048;
  cfg.buffer.out_size = 2048;
  cfg.task.stack_size = 6144;

  client_ = esp_mqtt_client_init(&cfg);
  if (client_ == nullptr) return false;
  esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, &Mqtt::onEvent, this);
  if (esp_mqtt_client_start(client_) != ESP_OK) {
    esp_mqtt_client_destroy(client_);
    client_ = nullptr;
    return false;
  }
  return true;
}

void Mqtt::stop() {
  if (client_ == nullptr) return;
  if (connected_) esp_mqtt_client_disconnect(client_);  // clean disconnect: no last will
  esp_mqtt_client_stop(client_);
  esp_mqtt_client_destroy(client_);
  client_ = nullptr;
  connected_ = false;
  std::lock_guard<std::mutex> lock(mutex_);
  unacked_.clear();
  ackedEarly_.clear();
}

bool Mqtt::publish(const std::string& topic, const std::string& payload, bool retain, int qos) {
  if (client_ == nullptr || !connected_) return false;
  const int id = esp_mqtt_client_publish(client_, topic.c_str(), payload.data(),
                                         static_cast<int>(payload.size()), qos, retain ? 1 : 0);
  if (id < 0) return false;
  if (qos > 0) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ackedEarly_.erase(id) == 0) unacked_.insert(id);
  }
  return true;
}

bool Mqtt::subscribe(const std::string& filter, int qos) {
  if (client_ == nullptr || !connected_) return false;
  return esp_mqtt_client_subscribe_single(client_, filter.c_str(), qos) >= 0;
}

bool Mqtt::poll(MqttMessage& out) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (inbox_.empty()) return false;
  out = std::move(inbox_.front());
  inbox_.pop_front();
  return true;
}

size_t Mqtt::unacked() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return unacked_.size();
}

void Mqtt::onEvent(void* arg, esp_event_base_t, int32_t, void* data) {
  static_cast<Mqtt*>(arg)->handle(static_cast<esp_mqtt_event_handle_t>(data));
}

void Mqtt::handle(esp_mqtt_event_handle_t event) {
  switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
      connected_ = true;
      ++connectCount_;
      break;
    case MQTT_EVENT_DISCONNECTED:
      connected_ = false;
      break;
    case MQTT_EVENT_PUBLISHED: {
      std::lock_guard<std::mutex> lock(mutex_);
      if (unacked_.erase(event->msg_id) == 0) ackedEarly_.insert(event->msg_id);
      break;
    }
    case MQTT_EVENT_DATA: {
      std::lock_guard<std::mutex> lock(mutex_);
      if (event->current_data_offset == 0) {
        partial_.topic.assign(event->topic, static_cast<size_t>(event->topic_len));
        partial_.payload.clear();
        partial_.payload.reserve(static_cast<size_t>(event->total_data_len));
      }
      partial_.payload.append(event->data, static_cast<size_t>(event->data_len));
      if (event->current_data_offset + event->data_len >= event->total_data_len) {
        if (inbox_.size() >= kInboxLimit) inbox_.pop_front();
        inbox_.push_back(std::move(partial_));
        partial_ = MqttMessage();
      }
      break;
    }
    default:
      break;
  }
}
