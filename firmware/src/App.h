#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "hw/LedStrip.h"
#include "lb/Canvas.h"
#include "lb/Message.h"
#include "lb/MessageQueue.h"
#include "lb/Output.h"
#include "lb/Player.h"
#include "lb/Policy.h"
#include "lb/Settings.h"
#include "lb/Topics.h"
#include "net/Mqtt.h"
#include "net/Net.h"

// The sign's life on each wake:
//
//   boot -> connect Wi-Fi (cached AP) -> MQTT -> sync retained state
//        -> show waiting messages if someone is there -> linger -> deep sleep
//
// Home Assistant never talks to the sign directly. It leaves retained MQTT
// messages; the sign picks them up whenever it's awake. That's what lets it
// spend nearly all its time in deep sleep and still respond when someone
// walks up to it.
class App {
 public:
  App();
  void setup();
  void loop();

 private:
  enum class Phase { Connecting, Syncing, Running };

  // Connection
  void stepConnecting(uint32_t now);
  bool startMqtt();
  void onMqttConnected();
  void subscribe();
  void failConnect(const char* what);
  void onSynced();

  // Incoming MQTT
  void drainInbox();
  void handleMessage(const std::string& slot, const std::string& payload);
  void handleSetting(std::string_view key, const std::string& payload);
  void runCommand();

  // Running
  void stepRunning(uint32_t now);
  void pollMotion(uint32_t now);
  void noteMotion(uint32_t now);
  void expireMessages();
  void queueShowableMessages(uint32_t now);
  void beginShow();
  void endShow();
  void renderFrame();
  void onShown(const lb::Message& msg);
  void showNow(const lb::Message& msg);
  void showBlocking(const lb::Message& msg);
  bool canLight() const;
  void clearSlot(const std::string& slot);
  void checkMaintenance(uint32_t now);
  void startOta();

  // Outgoing MQTT
  void publishState();
  void publishDiscovery();
  void publishMissingSettings();
  void flush(uint32_t timeoutMs);

  lb::PlayDefaults playDefaults() const;
  lb::AwakeState awakeState(uint32_t now) const;
  void idle();
  [[noreturn]] void goToSleep();

  lb::Topics topics_;
  std::string hostname_;
  Mqtt mqtt_;
  net::NtpClient ntp_;
  LedStrip leds_;
  lb::MatrixLayout layout_;
  lb::Canvas canvas_;
  lb::OutputStage output_;
  lb::LedPowerModel powerModel_;
  std::vector<uint8_t> wire_;
  lb::MessageQueue queue_;
  lb::Playlist playlist_;
  lb::Settings settings_;

  lb::WakeReason wake_ = lb::WakeReason::PowerOn;
  Phase phase_ = Phase::Connecting;
  uint32_t phaseStartMs_ = 0;
  uint16_t batteryMv_ = 0;
  lb::BatteryLevel battery_ = lb::BatteryLevel::Unknown;
  bool usb_ = false;
  bool alwaysOn_ = false;

  bool scanFallback_ = false;
  bool mqttStarted_ = false;
  uint32_t mqttStartMs_ = 0;
  uint32_t handledConnects_ = 0;
  std::string syncToken_;
  bool synced_ = false;
  uint32_t settingsSeen_ = 0;  // bit per SettingId received this wake
  bool connectFailedShown_ = false;

  bool pirHigh_ = false;
  bool pirStuck_ = false;       // on since before this wake; wait for it to reset
  uint32_t pirHighSinceMs_ = 0;
  bool motionSeen_ = false;
  uint32_t lastMotionMs_ = 0;
  uint32_t lastMotionReportMs_ = 0;
  uint32_t lastActivityMs_ = 0;
  uint32_t lastExpiryCheckMs_ = 0;

  std::set<uint32_t> shownSeqs_;  // messages already shown to this visitor
  bool showing_ = false;
  bool shownAny_ = false;
  bool batteryHintShown_ = false;
  bool wiring_ = false;
  uint32_t wiringStartMs_ = 0;
  uint32_t nextFrameUs_ = 0;

  bool ntpDue_ = false;
  std::string pendingCommand_;
  bool stateDirty_ = true;
  bool otaStarted_ = false;
  bool otaBusy_ = false;
  uint32_t maintenanceSinceMs_ = 0;
};
