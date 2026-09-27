#include "net/Net.h"

#include <Arduino.h>
#include <WiFi.h>
#include <sys/time.h>

#include <cstring>
#include <ctime>

#include "config.h"

namespace net {

namespace {

constexpr uint32_t kNtpTimeoutMs = 1500;
constexpr uint8_t kNtpAttempts = 2;
constexpr uint32_t kNtpToUnix = 2208988800UL;  // seconds from 1900 to 1970
constexpr time_t kPlausibleTime = 1735689600;  // 2025-01-01

void applyStaticIp() {
#ifdef LB_STATIC_IP
  IPAddress ip, gateway, subnet, dns;
  ip.fromString(LB_STATIC_IP);
  gateway.fromString(LB_GATEWAY);
  subnet.fromString(LB_SUBNET);
  dns.fromString(LB_DNS);
  WiFi.config(ip, gateway, subnet, dns);
#endif
}

}  // namespace

void startWifi(const ApCache& cache, const char* hostname) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(true);  // modem sleep between DTIM beacons while we linger
  applyStaticIp();
  if (cache.valid) {
    WiFi.begin(LB_WIFI_SSID, LB_WIFI_PASSWORD, cache.channel, cache.bssid, true);
  } else {
    WiFi.begin(LB_WIFI_SSID, LB_WIFI_PASSWORD);
  }
}

void restartWifiWithScan() {
  WiFi.disconnect(false, false);
  applyStaticIp();
  WiFi.begin(LB_WIFI_SSID, LB_WIFI_PASSWORD);
}

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }

void rememberAp(ApCache& cache) {
  const uint8_t* bssid = WiFi.BSSID();
  if (bssid == nullptr) return;
  std::memcpy(cache.bssid, bssid, sizeof(cache.bssid));
  cache.channel = static_cast<uint8_t>(WiFi.channel());
  cache.valid = cache.channel != 0;
}

void stopWifi() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
}

int rssi() { return WiFi.RSSI(); }

bool clockIsSet() { return std::time(nullptr) > kPlausibleTime; }

void setClockAtLeast(int64_t unixSeconds) {
  if (unixSeconds <= kPlausibleTime || unixSeconds <= static_cast<int64_t>(std::time(nullptr))) return;
  timeval tv;
  tv.tv_sec = static_cast<time_t>(unixSeconds);
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

void NtpClient::start(const char* server) {
  server_ = server;
  attempts_ = 0;
  active_ = udp_.begin(0) && send();
}

bool NtpClient::send() {
  uint8_t packet[48] = {};
  packet[0] = 0x23;  // LI 0, version 4, client mode
  if (!udp_.beginPacket(server_, 123)) return false;
  udp_.write(packet, sizeof(packet));
  if (!udp_.endPacket()) return false;
  sentAtMs_ = millis();
  ++attempts_;
  return true;
}

bool NtpClient::poll() {
  if (!active_) return false;
  if (udp_.parsePacket() >= 48) {
    uint8_t reply[48];
    udp_.read(reply, sizeof(reply));
    const uint32_t seconds = (uint32_t(reply[40]) << 24) | (uint32_t(reply[41]) << 16) |
                             (uint32_t(reply[42]) << 8) | uint32_t(reply[43]);
    const uint32_t fraction = (uint32_t(reply[44]) << 24) | (uint32_t(reply[45]) << 16) |
                              (uint32_t(reply[46]) << 8) | uint32_t(reply[47]);
    active_ = false;
    udp_.stop();
    if (seconds <= kNtpToUnix) return false;
    timeval tv;
    tv.tv_sec = static_cast<time_t>(seconds - kNtpToUnix);
    tv.tv_usec = static_cast<suseconds_t>((uint64_t(fraction) * 1000000ULL) >> 32);
    settimeofday(&tv, nullptr);
    return true;
  }
  if (millis() - sentAtMs_ > kNtpTimeoutMs) {
    if (attempts_ < kNtpAttempts && send()) return false;
    active_ = false;
    udp_.stop();
  }
  return false;
}

}  // namespace net
