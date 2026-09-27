#include "hw/Board.h"

#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <soc/soc_caps.h>
#include <sys/time.h>

#include "config.h"

namespace board {

lb::WakeReason wakeReason() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:
    case ESP_SLEEP_WAKEUP_GPIO:
      return lb::WakeReason::Motion;
    case ESP_SLEEP_WAKEUP_TIMER:
      return lb::WakeReason::Timer;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      return lb::WakeReason::PowerOn;  // reset, power-up or brown-out
    default:
      return lb::WakeReason::Other;
  }
}

void begin() {
  if (LB_PIN_PIR >= 0) {
#if SOC_RTCIO_INPUT_OUTPUT_SUPPORTED
    // The pin was an RTC input during sleep; hand it back to the digital GPIO matrix.
    if (rtc_gpio_is_valid_gpio(static_cast<gpio_num_t>(LB_PIN_PIR))) {
      rtc_gpio_deinit(static_cast<gpio_num_t>(LB_PIN_PIR));
    }
#endif
    pinMode(LB_PIN_PIR, LB_PIR_ACTIVE_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
  }
  if (LB_PIN_VBUS >= 0) pinMode(LB_PIN_VBUS, INPUT);
  if (LB_PIN_VBAT >= 0) analogSetPinAttenuation(LB_PIN_VBAT, ADC_11db);
}

bool hasMotionSensor() { return LB_PIN_PIR >= 0; }

bool motionNow() {
  if (LB_PIN_PIR < 0) return false;
  return digitalRead(LB_PIN_PIR) == (LB_PIR_ACTIVE_HIGH ? HIGH : LOW);
}

bool hasBatterySense() { return LB_PIN_VBAT >= 0; }

uint16_t batteryMillivolts() {
  if (LB_PIN_VBAT < 0) return 0;
  constexpr int kSamples = 16;
  uint32_t sum = 0;
  for (int i = 0; i < kSamples; ++i) sum += analogReadMilliVolts(LB_PIN_VBAT);
  const float mv = static_cast<float>(sum) / kSamples * LB_VBAT_DIVIDER;
  return static_cast<uint16_t>(mv + 0.5f);
}

bool hasUsbSense() { return LB_PIN_VBUS >= 0; }

bool usbPower() { return LB_PIN_VBUS >= 0 && digitalRead(LB_PIN_VBUS) == HIGH; }

int64_t rtcSeconds() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<int64_t>(tv.tv_sec);
}

void deepSleep(uint32_t seconds, bool armMotion) {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (seconds > 0) esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  if (armMotion && LB_PIN_PIR >= 0) {
    const gpio_num_t pin = static_cast<gpio_num_t>(LB_PIN_PIR);
#if SOC_PM_SUPPORT_EXT0_WAKEUP
    esp_sleep_enable_ext0_wakeup(pin, LB_PIR_ACTIVE_HIGH ? 1 : 0);
    // Keep the input from floating if the sensor is unplugged.
    if (LB_PIR_ACTIVE_HIGH) {
      rtc_gpio_pullup_dis(pin);
      rtc_gpio_pulldown_en(pin);
    } else {
      rtc_gpio_pulldown_dis(pin);
      rtc_gpio_pullup_en(pin);
    }
#elif SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
    esp_deep_sleep_enable_gpio_wakeup(1ULL << LB_PIN_PIR,
                                      LB_PIR_ACTIVE_HIGH ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW);
#endif
  }
  esp_deep_sleep_start();
}

}  // namespace board
