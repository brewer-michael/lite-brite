#include "App.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <esp_random.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <type_traits>

#include "config.h"
#include "hw/Board.h"
#include "lb/Battery.h"
#include "lb/HomeAssistant.h"
#include "lb/Render.h"
#include "lb/Version.h"

#if LB_DEBUG
#define LOG(fmt, ...) Serial.printf("[%6lu] " fmt "\n", static_cast<unsigned long>(millis()), ##__VA_ARGS__)
#else
#define LOG(...) \
  do {           \
  } while (0)
#endif

namespace {

constexpr uint32_t kRtcMagic = 0x4C425254;  // "LBRT"
constexpr uint32_t kRtcVersion = 1;
constexpr uint32_t kFastConnectMs = 4000;  // then retry with a full scan
constexpr uint32_t kMqttTimeoutMs = 6000;
constexpr uint32_t kSyncTimeoutMs = 2000;
constexpr uint32_t kFlushTimeoutMs = 2000;
constexpr uint32_t kRearmSeconds = 8;
constexpr uint32_t kMotionReportMs = 10000;
constexpr uint32_t kExpiryCheckMs = 5000;
constexpr uint32_t kWiringTestMs = 20000;
constexpr int64_t kNtpResyncSeconds = 6 * 3600;
constexpr int64_t kDiscoveryRefreshSeconds = 24 * 3600;
constexpr lb::ColorOrder kColorOrder = lb::ColorOrder::LB_LED_COLOR_ORDER;

// Survives deep sleep (not power loss). Plain data only, so no constructor
// runs at boot and wipes it; a magic number marks it valid.
struct RtcState {
  uint32_t magic;
  uint32_t version;
  uint32_t wakes;
  net::ApCache ap;
  uint8_t settings[sizeof(lb::Settings)];
  uint8_t batteryLevel;
  bool rearmPending;
  bool discoveryDone;
  int64_t checkInAt;
  int64_t lastNtpSync;
  int64_t lastDiscovery;
};
static_assert(std::is_trivially_copyable<lb::Settings>::value, "Settings is kept as raw bytes");
static_assert(std::is_trivial<RtcState>::value, "RtcState must not need a constructor");

RTC_NOINIT_ATTR RtcState g_rtc;

lb::Settings loadSettings() {
  lb::Settings s;
  std::memcpy(&s, g_rtc.settings, sizeof(s));
  return s;
}

void storeSettings(const lb::Settings& s) { std::memcpy(g_rtc.settings, &s, sizeof(s)); }

lb::BatteryThresholds thresholds() {
  lb::BatteryThresholds t;
  t.lowMv = LB_BATT_LOW_MV;
  t.criticalMv = LB_BATT_CRITICAL_MV;
  t.emptyMv = LB_BATT_EMPTY_MV;
  return t;
}

lb::Message internalMessage(const char* slot, const char* text, lb::Rgb color, lb::Effect effect,
                            uint16_t seconds) {
  lb::Message m;
  m.slot = slot;  // a leading '_' marks messages that never touch MQTT
  m.text = text;
  m.color = color;
  m.effect = effect;
  m.durationS = seconds;
  m.when = lb::ShowWhen::Now;
  m.once = false;
  return m;
}

std::string lowerTrim(const std::string& s) {
  size_t a = 0;
  size_t b = s.size();
  while (a < b && isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  std::string out = s.substr(a, b - a);
  for (char& c : out) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return out;
}

}  // namespace

App::App()
    : topics_(LB_TOPIC_PREFIX, LB_DEVICE_ID),
      hostname_(std::string("lite-brite-") + LB_DEVICE_ID),
      leds_(LB_PIN_LED_DATA, LB_PIN_LED_POWER, LB_LED_POWER_ACTIVE_HIGH, LB_MATRIX_WIDTH * LB_MATRIX_HEIGHT),
      canvas_(LB_MATRIX_WIDTH, LB_MATRIX_HEIGHT),
      wire_(LB_MATRIX_WIDTH * LB_MATRIX_HEIGHT * 3) {
  layout_.width = LB_MATRIX_WIDTH;
  layout_.height = LB_MATRIX_HEIGHT;
  layout_.columnMajor = LB_MATRIX_COLUMN_MAJOR;
  layout_.serpentine = LB_MATRIX_SERPENTINE;
  layout_.flipX = LB_MATRIX_FLIP_X;
  layout_.flipY = LB_MATRIX_FLIP_Y;
  powerModel_.maPerChannel = LB_LED_MA_PER_CHANNEL;
  powerModel_.idleUaPerLed = LB_LED_IDLE_UA;
}

// ---------------------------------------------------------------------------
// Boot

void App::setup() {
#if LB_DEBUG
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
  Serial.setTxTimeoutMs(0);  // never stall when no computer is listening
#endif
#endif
  wake_ = board::wakeReason();
  if (g_rtc.magic != kRtcMagic || g_rtc.version != kRtcVersion) {
    std::memset(&g_rtc, 0, sizeof(g_rtc));
    g_rtc.magic = kRtcMagic;
    g_rtc.version = kRtcVersion;
    storeSettings(lb::Settings{});
  }
  ++g_rtc.wakes;
  settings_ = loadSettings();
  board::begin();

  // A timer wake that only exists to re-arm the motion sensor (it was still
  // triggered when we went to sleep). Decide in a few milliseconds, no radio.
  if (wake_ == lb::WakeReason::Timer && g_rtc.rearmPending) {
    g_rtc.rearmPending = false;
    const int64_t remaining = g_rtc.checkInAt - board::rtcSeconds();
    if (remaining > static_cast<int64_t>(kRearmSeconds)) {
      if (board::motionNow()) {
        g_rtc.rearmPending = true;
        board::deepSleep(kRearmSeconds, false);
      }
      board::deepSleep(static_cast<uint32_t>(remaining), true);
    }
    // Otherwise the regular check-in is due: carry on.
  }

  leds_.begin();
  // Read the battery before the radio starts drawing current.
  batteryMv_ = board::batteryMillivolts();
  battery_ = lb::batteryLevel(batteryMv_, thresholds(), static_cast<lb::BatteryLevel>(g_rtc.batteryLevel));
  g_rtc.batteryLevel = static_cast<uint8_t>(battery_);
  usb_ = board::usbPower();
  alwaysOn_ = LB_ALWAYS_POWERED || usb_;
  if (wake_ == lb::WakeReason::Motion || board::motionNow()) noteMotion(millis());

  LOG("lite-brite %s: wake=%s #%lu battery=%umV (%s) usb=%d", lb::kFirmwareVersion,
      lb::wakeReasonName(wake_), static_cast<unsigned long>(g_rtc.wakes), batteryMv_,
      lb::batteryLevelName(battery_), usb_);

  playlist_.configure(playDefaults(), LB_MATRIX_WIDTH, LB_MATRIX_HEIGHT);
  net::startWifi(g_rtc.ap, hostname_.c_str());
  phase_ = Phase::Connecting;
  phaseStartMs_ = millis();
}

void App::loop() {
  const uint32_t now = millis();
  if (board::motionNow()) noteMotion(now);

  switch (phase_) {
    case Phase::Connecting:
      stepConnecting(now);
      break;
    case Phase::Syncing:
      drainInbox();
      if (synced_ || now - phaseStartMs_ > kSyncTimeoutMs) onSynced();
      break;
    case Phase::Running:
      stepRunning(now);
      break;
  }
  renderFrame();
  idle();
}

// ---------------------------------------------------------------------------
// Connecting

void App::stepConnecting(uint32_t now) {
  const uint32_t elapsed = now - phaseStartMs_;
  if (!net::wifiConnected()) {
    if (g_rtc.ap.valid && !scanFallback_ && elapsed > kFastConnectMs) {
      LOG("cached access point didn't answer; scanning");
      g_rtc.ap.valid = false;
      scanFallback_ = true;
      net::restartWifiWithScan();
    }
    const uint32_t timeout = wake_ == lb::WakeReason::PowerOn ? 2 * LB_WIFI_TIMEOUT_MS : LB_WIFI_TIMEOUT_MS;
    if (elapsed > timeout) failConnect("Wi-Fi");
    return;
  }
  if (!mqttStarted_) {
    net::rememberAp(g_rtc.ap);
    LOG("Wi-Fi up in %lums, rssi %d", static_cast<unsigned long>(elapsed), net::rssi());
    mqttStarted_ = startMqtt();
    mqttStartMs_ = now;
    if (!mqttStarted_) failConnect("MQTT");
    return;
  }
  if (mqtt_.connected()) {
    onMqttConnected();
  } else if (now - mqttStartMs_ > kMqttTimeoutMs) {
    failConnect("MQTT");
  }
}

bool App::startMqtt() {
  static std::string willTopic;
  willTopic = topics_.awake();
  Mqtt::Config cfg;
  cfg.host = LB_MQTT_HOST;
  cfg.port = LB_MQTT_PORT;
  cfg.user = LB_MQTT_USER;
  cfg.password = LB_MQTT_PASSWORD;
  cfg.clientId = hostname_.c_str();
  cfg.willTopic = willTopic.c_str();
  return mqtt_.start(cfg);
}

void App::onMqttConnected() {
  LOG("MQTT connected");
  handledConnects_ = mqtt_.connectCount();
  subscribe();
  // Anything published before this marker comes back first, so its echo
  // means every retained message and setting has arrived.
  syncToken_ = std::to_string(esp_random());
  mqtt_.publish(topics_.sync(), syncToken_, false, 1);
  phase_ = Phase::Syncing;
  phaseStartMs_ = millis();
}

void App::subscribe() {
  mqtt_.publish(topics_.awake(), "ON", true, 1);
  if (motionSeen_) {
    mqtt_.publish(topics_.motion(), "ON", false, 0);
    lastMotionReportMs_ = millis();
  }
  mqtt_.subscribe(topics_.messageFilter());
  mqtt_.subscribe(topics_.settingFilter());
  mqtt_.subscribe(topics_.command());
  mqtt_.subscribe(topics_.sync());
}

void App::failConnect(const char* what) {
  LOG("%s connection failed", what);
  const bool wifi = std::strcmp(what, "Wi-Fi") == 0;
  if (wake_ == lb::WakeReason::PowerOn && !connectFailedShown_) {
    // Setup feedback without a serial cable.
    connectFailedShown_ = true;
    showBlocking(wifi ? internalMessage("_status", ":wifi: No Wi-Fi", lb::colors::kRed, lb::Effect::Scroll, 6)
                      : internalMessage("_status", ":x: No MQTT", lb::colors::kOrange, lb::Effect::Scroll, 6));
  }
  if (alwaysOn_) {
    phaseStartMs_ = millis();  // Wi-Fi and MQTT keep retrying on their own
    mqttStartMs_ = millis();
    return;
  }
  goToSleep();
}

void App::onSynced() {
  LOG("%s: %u message(s) waiting", synced_ ? "synced" : "sync timed out", static_cast<unsigned>(queue_.size()));
  phase_ = Phase::Running;
  phaseStartMs_ = millis();
  const int64_t nowS = board::rtcSeconds();
  if (!g_rtc.discoveryDone || wake_ == lb::WakeReason::PowerOn ||
      nowS - g_rtc.lastDiscovery > kDiscoveryRefreshSeconds) {
    publishDiscovery();
  }
  publishMissingSettings();
  expireMessages();
  publishState();
  if (!net::clockIsSet() || nowS - g_rtc.lastNtpSync > kNtpResyncSeconds) ntp_.start(LB_NTP_SERVER);
  if (wake_ == lb::WakeReason::PowerOn) {
    showNow(internalMessage("_status", ":check: OK!", lb::colors::kGreen, lb::Effect::Static, 3));
  }
  setCpuFrequencyMhz(LB_CPU_MHZ_IDLE);
}

// ---------------------------------------------------------------------------
// Incoming MQTT

void App::drainInbox() {
  MqttMessage msg;
  while (mqtt_.poll(msg)) {
    std::string_view rest;
    switch (topics_.classify(msg.topic, rest)) {
      case lb::Topics::Kind::Message:
        handleMessage(std::string(rest), msg.payload);
        break;
      case lb::Topics::Kind::Setting:
        handleSetting(rest, msg.payload);
        break;
      case lb::Topics::Kind::Command:
        if (!msg.payload.empty()) pendingCommand_ = msg.payload;
        break;
      case lb::Topics::Kind::Sync:
        if (msg.payload == syncToken_) synced_ = true;
        break;
      case lb::Topics::Kind::Unknown:
        break;
    }
  }
}

void App::handleMessage(const std::string& slot, const std::string& payload) {
  if (payload.empty()) {  // cleared by Home Assistant (or our own echo)
    queue_.remove(slot);
    if (playlist_.remove(slot)) LOG("'%s' withdrawn", slot.c_str());
    stateDirty_ = true;
    return;
  }
  lb::Message msg;
  std::string error;
  if (!lb::parseMessage(slot, payload, msg, error)) {
    LOG("ignoring '%s': %s", slot.c_str(), error.c_str());
    mqtt_.publish(topics_.event(), lb::invalidEventPayload(slot, error), false, 0);
    return;
  }
  if (!queue_.upsert(msg)) {
    LOG("queue full: dropped '%s'", slot.c_str());
    return;
  }
  LOG("message '%s': %s", slot.c_str(), msg.text.c_str());
  if (playlist_.contains(slot)) playlist_.add(*queue_.find(slot));  // update what's on screen
  stateDirty_ = true;
}

void App::handleSetting(std::string_view key, const std::string& payload) {
  lb::SettingId id;
  if (!lb::findSetting(key, id) || payload.empty()) return;
  settingsSeen_ |= 1u << static_cast<unsigned>(id);
  const bool wasAwakeMode = settings_.stayAwake;
  if (!lb::applySetting(settings_, id, payload)) {
    LOG("bad value for %.*s: %s", static_cast<int>(key.size()), key.data(), payload.c_str());
    return;
  }
  storeSettings(settings_);
  playlist_.configure(playDefaults(), LB_MATRIX_WIDTH, LB_MATRIX_HEIGHT);
  if (id == lb::SettingId::StayAwake && settings_.stayAwake && !wasAwakeMode) {
    maintenanceSinceMs_ = millis();
    LOG("maintenance mode on");
  }
  // Tidy values like "75.0" so Home Assistant shows what the sign uses.
  const std::string canonical = lb::settingValue(settings_, id);
  if (canonical != payload) mqtt_.publish(topics_.setting(std::string(key)), canonical, true, 1);
}

void App::runCommand() {
  const std::string cmd = lowerTrim(pendingCommand_);
  pendingCommand_.clear();
  mqtt_.publish(topics_.command(), "", true, 1);  // consume the retained command
  LOG("command: %s", cmd.c_str());
  if (cmd == "clear") {
    std::vector<std::string> slots;
    for (const auto& m : queue_.items()) slots.push_back(m.slot);
    for (const auto& s : slots) clearSlot(s);
    playlist_.clear();
    if (showing_) endShow();
  } else if (cmd == "test") {
    lb::Message test = internalMessage("_test", ":smile: Hello from lite-brite! :heart:", lb::colors::kWhite,
                                       lb::Effect::Flash, 12);
    test.rainbow = true;
    showNow(test);
  } else if (cmd == "wiring") {
    playlist_.clear();
    wiring_ = true;
    wiringStartMs_ = millis();
    beginShow();
  } else if (cmd == "reboot") {
    flush(kFlushTimeoutMs);
    ESP.restart();
  }
}

// ---------------------------------------------------------------------------
// Running

void App::stepRunning(uint32_t now) {
  if (mqtt_.connectCount() != handledConnects_ && mqtt_.connected()) {
    LOG("MQTT reconnected");  // clean session: subscribe again
    handledConnects_ = mqtt_.connectCount();
    subscribe();
  }
  drainInbox();
  if (ntp_.active() && ntp_.poll()) {
    g_rtc.lastNtpSync = board::rtcSeconds();
    LOG("clock set");
  }
  if (!pendingCommand_.empty()) runCommand();
  if (now - lastExpiryCheckMs_ > kExpiryCheckMs) {
    lastExpiryCheckMs_ = now;
    expireMessages();
  }
  checkMaintenance(now);
  if (otaStarted_) ArduinoOTA.handle();
  queueShowableMessages(now);
  if (stateDirty_ && !showing_) publishState();
  if (!lb::keepAwake(awakeState(now))) goToSleep();
}

void App::noteMotion(uint32_t now) {
  // A fresh visit (motion after a quiet spell) may see messages again.
  if (!motionSeen_ || now - lastMotionMs_ > lb::kPresenceWindowMs) shownSeqs_.clear();
  motionSeen_ = true;
  lastMotionMs_ = now;
  if (mqtt_.connected() && now - lastMotionReportMs_ > kMotionReportMs) {
    mqtt_.publish(topics_.motion(), "ON", false, 0);
    lastMotionReportMs_ = now;
  }
}

void App::expireMessages() {
  if (!net::clockIsSet()) return;
  for (const std::string& slot : queue_.expired(board::rtcSeconds())) {
    if (const lb::Message* m = queue_.find(slot)) {
      LOG("'%s' expired", slot.c_str());
      mqtt_.publish(topics_.event(), lb::eventPayload("expired", *m), false, 0);
    }
    playlist_.remove(slot);
    clearSlot(slot);
  }
}

void App::queueShowableMessages(uint32_t now) {
  if (wiring_ || !canLight()) return;
  const bool present = lb::someonePresent(awakeState(now));
  const int64_t clock = net::clockIsSet() ? board::rtcSeconds() : 0;
  for (const lb::Message* m : queue_.ordered(clock)) {
    if (shownSeqs_.count(m->seq) != 0 || playlist_.contains(m->slot)) continue;
    if (!lb::mayShow(*m, present, board::hasMotionSensor())) continue;
    playlist_.add(*m);
  }
  if (playlist_.active() && !showing_) beginShow();
}

bool App::canLight() const {
  return usb_ || LB_ALWAYS_POWERED ||
         (battery_ != lb::BatteryLevel::Critical && battery_ != lb::BatteryLevel::Empty);
}

void App::showNow(const lb::Message& msg) {
  if (!canLight()) return;
  playlist_.add(msg);
  beginShow();
}

void App::beginShow() {
  if (showing_) return;
  if (!leds_.powerOn()) {
    LOG("LED power-on failed");
    playlist_.clear();
    wiring_ = false;
    return;
  }
  showing_ = true;
  nextFrameUs_ = micros();
}

void App::endShow() {
  leds_.powerOff();
  showing_ = false;
  wiring_ = false;
  canvas_.clear();
  lastActivityMs_ = millis();
  stateDirty_ = true;
}

void App::renderFrame() {
  if (!showing_) return;
  const uint32_t nowUs = micros();
  if (static_cast<int32_t>(nowUs - nextFrameUs_) < 0) return;

  const uint32_t frameUs = nextFrameUs_;  // render the scheduled time: even scroll steps
  uint32_t interval = 40000;
  uint8_t percent = 30;
  if (wiring_) {
    lb::drawWiringTest(canvas_, millis() - wiringStartMs_);
    if (millis() - wiringStartMs_ > kWiringTestMs) {
      endShow();
      return;
    }
  } else {
    lb::Message finished;
    if (playlist_.update(canvas_, frameUs, &finished)) onShown(finished);
    if (!playlist_.active()) {
      if (battery_ == lb::BatteryLevel::Low && shownAny_ && !batteryHintShown_) {
        batteryHintShown_ = true;  // one gentle "charge me" after the messages
        playlist_.add(internalMessage("_battery", ":battery_low:", lb::colors::kRed, lb::Effect::Blink, 3));
      } else {
        endShow();
        return;
      }
    }
    interval = playlist_.frameIntervalUs();
    percent = playlist_.brightness();
  }

  const uint32_t budget =
      (usb_ || LB_ALWAYS_POWERED) ? LB_LED_MAX_MA : lb::ledBudgetMa(batteryMv_, LB_LED_MAX_MA, thresholds());
  output_.render(canvas_, layout_, kColorOrder, lb::brightnessLevel(percent), budget, powerModel_, wire_.data());
  leds_.show(wire_.data(), wire_.size());

  nextFrameUs_ += interval;
  const uint32_t after = micros();
  if (static_cast<int32_t>(after - nextFrameUs_) > 0) {
    // Fell behind (a slow publish, say): skip frames rather than rush them.
    nextFrameUs_ += ((after - nextFrameUs_) / interval + 1) * interval;
  }
}

void App::onShown(const lb::Message& msg) {
  lastActivityMs_ = millis();
  if (msg.slot.empty() || msg.slot[0] == '_') return;  // internal status/test messages
  LOG("shown '%s'", msg.slot.c_str());
  shownSeqs_.insert(msg.seq);
  shownAny_ = true;
  mqtt_.publish(topics_.event(), lb::eventPayload("shown", msg), false, 0);
  if (msg.once) {
    // Only clear it if Home Assistant hasn't replaced it in the meantime.
    const lb::Message* current = queue_.find(msg.slot);
    if (current != nullptr && current->seq == msg.seq) clearSlot(msg.slot);
  }
  stateDirty_ = true;
}

void App::showBlocking(const lb::Message& msg) {
  showNow(msg);
  while (showing_) {
    renderFrame();
    delay(1);
  }
}

void App::clearSlot(const std::string& slot) {
  mqtt_.publish(topics_.message(slot), "", true, 1);  // deletes the retained message
  queue_.remove(slot);
  stateDirty_ = true;
}

void App::checkMaintenance(uint32_t now) {
  if (!settings_.stayAwake) return;
  if (!otaStarted_) startOta();
  if (!alwaysOn_ && now - maintenanceSinceMs_ > LB_MAINTENANCE_MAX_MIN * 60000UL) {
    LOG("maintenance mode timed out");
    settings_.stayAwake = false;
    storeSettings(settings_);
    mqtt_.publish(topics_.setting("stay_awake"), "OFF", true, 1);
  }
}

void App::startOta() {
  ArduinoOTA.setHostname(hostname_.c_str());
  ArduinoOTA.setPassword(LB_OTA_PASSWORD);
  ArduinoOTA.onStart([this]() {
    otaBusy_ = true;
    if (showing_) endShow();
    LOG("OTA update starting");
  });
  ArduinoOTA.onEnd([this]() { otaBusy_ = false; });
  ArduinoOTA.onError([this](ota_error_t error) {
    otaBusy_ = false;
    LOG("OTA error %d", static_cast<int>(error));
  });
  ArduinoOTA.begin();
  otaStarted_ = true;
  LOG("OTA ready at %s.local", hostname_.c_str());
}

// ---------------------------------------------------------------------------
// Outgoing MQTT

void App::publishState() {
  lb::DeviceStatus s;
  if (board::hasBatterySense()) {
    s.batteryPercent = lb::batteryPercent(batteryMv_);
    s.millivolts = batteryMv_;
  }
  s.batteryLevel = lb::batteryLevelName(battery_);
  s.rssi = net::rssi();
  s.wake = lb::wakeReasonName(wake_);
  s.pending = queue_.size();
  s.usb = usb_;
  s.wakes = g_rtc.wakes;
  s.version = lb::kFirmwareVersion;
  mqtt_.publish(topics_.state(), lb::statePayload(s), true, 1);
  stateDirty_ = false;
}

void App::publishDiscovery() {
  lb::DeviceInfo info;
  info.name = LB_DEVICE_NAME;
  info.model = "LED sign " + std::to_string(LB_MATRIX_WIDTH) + "x" + std::to_string(LB_MATRIX_HEIGHT);
  info.hardware = LB_BOARD_NAME;
  info.version = lb::kFirmwareVersion;
  info.supportUrl = lb::kProjectUrl;
  info.motionSensor = board::hasMotionSensor();
  info.batterySensor = board::hasBatterySense();
  info.usbSense = board::hasUsbSense();
  if (mqtt_.publish(topics_.discovery(LB_DISCOVERY_PREFIX), lb::discoveryPayload(topics_, info), true, 1)) {
    g_rtc.discoveryDone = true;
    g_rtc.lastDiscovery = board::rtcSeconds();
    LOG("published Home Assistant discovery");
  }
}

void App::publishMissingSettings() {
  // Settings that have never been set in Home Assistant get the sign's
  // current value, so the entities show something sensible.
  for (size_t i = 0; i < lb::kSettingCount; ++i) {
    if (settingsSeen_ & (1u << i)) continue;
    const lb::SettingInfo& info = lb::settingInfoAt(i);
    mqtt_.publish(topics_.setting(info.key), lb::settingValue(settings_, info.id), true, 1);
  }
}

void App::flush(uint32_t timeoutMs) {
  const uint32_t start = millis();
  while (mqtt_.connected() && mqtt_.unacked() > 0 && millis() - start < timeoutMs) delay(10);
}

// ---------------------------------------------------------------------------
// Power

lb::PlayDefaults App::playDefaults() const {
  lb::PlayDefaults d;
  d.speed = settings_.speed;
  d.durationS = settings_.duration;
  d.brightness = settings_.brightness;
  return d;
}

lb::AwakeState App::awakeState(uint32_t now) const {
  lb::AwakeState s;
  s.reason = wake_;
  s.nowMs = now;
  s.motionSeen = motionSeen_;
  s.lastMotionMs = lastMotionMs_;
  s.lastActivityMs = lastActivityMs_;
  s.busy = showing_ || otaBusy_ || ntp_.active() || !pendingCommand_.empty() || phase_ != Phase::Running;
  s.alwaysOn = alwaysOn_;
  s.stayAwake = settings_.stayAwake;
  s.lingerS = settings_.linger;
  return s;
}

void App::idle() {
  if (showing_) {
    const int32_t waitUs = static_cast<int32_t>(nextFrameUs_ - micros());
    if (waitUs > 2000) delay(std::min<int32_t>((waitUs - 1000) / 1000, 10));
  } else {
    delay(10);
  }
}

void App::goToSleep() {
  if (showing_) endShow();
  if (mqtt_.connected()) {
    publishState();
    mqtt_.publish(topics_.awake(), "OFF", true, 1);
    flush(kFlushTimeoutMs);
  }
  mqtt_.stop();
  net::stopWifi();
  leds_.holdOffDuringSleep();

  const lb::SleepPlan plan = lb::planSleep(settings_.wakeInterval, battery_, board::hasMotionSensor());
  g_rtc.checkInAt = board::rtcSeconds() + plan.timerSeconds;
  LOG("sleeping %lus (motion wake %s), awake for %lums", static_cast<unsigned long>(plan.timerSeconds),
      plan.armMotion ? "on" : "off", static_cast<unsigned long>(millis()));
#if LB_DEBUG
  Serial.flush();
#endif
  if (plan.armMotion && board::motionNow()) {
    // The sensor is still triggered; arming it now would wake us at once.
    // Nap briefly and re-arm once it has settled.
    g_rtc.rearmPending = true;
    board::deepSleep(kRearmSeconds, false);
  }
  g_rtc.rearmPending = false;
  board::deepSleep(plan.timerSeconds, plan.armMotion);
}
