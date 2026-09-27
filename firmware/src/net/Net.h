#pragma once

#include <WiFiUdp.h>

#include <cstdint>

namespace net {

// The access point we last connected to, kept in RTC memory across deep
// sleep so the next wake can skip the Wi-Fi scan.
struct ApCache {
  uint8_t bssid[6];
  uint8_t channel;
  bool valid;
};

// Starts connecting (non-blocking). Uses the cached AP when it has one.
void startWifi(const ApCache& cache, const char* hostname);
// Forgets the cached AP and connects with a full scan.
void restartWifiWithScan();
bool wifiConnected();
void rememberAp(ApCache& cache);  // call once connected
void stopWifi();
int rssi();

// A minimal SNTP client: one UDP request, no startup delay. Sets the system
// clock, which then keeps counting through deep sleep.
class NtpClient {
 public:
  void start(const char* server);
  // Returns true once the clock has been set. Gives up after a timeout.
  bool poll();
  bool active() const { return active_; }

 private:
  bool send();

  WiFiUDP udp_;
  const char* server_ = nullptr;
  bool active_ = false;
  uint8_t attempts_ = 0;
  uint32_t sentAtMs_ = 0;
};

// True once the clock holds a plausible current time.
bool clockIsSet();

// Moves the clock forward to `unixSeconds` if it's behind (never backward).
// Implausible times are ignored.
void setClockAtLeast(int64_t unixSeconds);

}  // namespace net
