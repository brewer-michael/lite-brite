#include "hw/LedStrip.h"

#include <Arduino.h>
#include <driver/gpio.h>

#include <cstring>

namespace {

constexpr uint32_t kResolutionHz = 10000000;  // 0.1 us per RMT tick
constexpr uint32_t kLatchUs = 300;            // WS2812B reset: > 280 us low
constexpr uint32_t kSupplySettleMs = 5;       // soft-start of the power switch
constexpr int kTxTimeoutMs = 100;

rmt_symbol_word_t symbol(uint16_t highTicks, uint16_t lowTicks) {
  rmt_symbol_word_t s;
  s.level0 = 1;
  s.duration0 = highTicks;
  s.level1 = 0;
  s.duration1 = lowTicks;
  return s;
}

}  // namespace

LedStrip::LedStrip(int dataPin, int powerPin, bool powerActiveHigh, size_t ledCount)
    : dataPin_(dataPin), powerPin_(powerPin), powerActiveHigh_(powerActiveHigh), ledCount_(ledCount) {}

void LedStrip::begin() {
  if (dataPin_ >= 0) gpio_hold_dis(static_cast<gpio_num_t>(dataPin_));
  if (powerPin_ >= 0) {
    gpio_hold_dis(static_cast<gpio_num_t>(powerPin_));
    pinMode(powerPin_, OUTPUT);
  }
  setPower(false);
  parkDataPin();
}

void LedStrip::holdOffDuringSleep() {
  powerOff();
  // Latch the supply switch off and the data line low while the chip sleeps.
  if (dataPin_ >= 0) gpio_hold_en(static_cast<gpio_num_t>(dataPin_));
  if (powerPin_ >= 0) gpio_hold_en(static_cast<gpio_num_t>(powerPin_));
  gpio_deep_sleep_hold_en();
}

void LedStrip::setPower(bool on) {
  if (powerPin_ < 0) return;
  digitalWrite(powerPin_, on == powerActiveHigh_ ? HIGH : LOW);
}

void LedStrip::parkDataPin() {
  if (dataPin_ < 0) return;
  const gpio_num_t pin = static_cast<gpio_num_t>(dataPin_);
  gpio_reset_pin(pin);  // this enables the pull-up, which could back-power the first LED
  gpio_pullup_dis(pin);
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  gpio_set_level(pin, 0);
}

bool LedStrip::powerOn() {
  if (on_) return true;
  if (dataPin_ < 0) return false;
  setPower(true);
  delay(kSupplySettleMs);

  rmt_tx_channel_config_t cfg = {};
  cfg.gpio_num = static_cast<gpio_num_t>(dataPin_);
  cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  cfg.resolution_hz = kResolutionHz;
  cfg.trans_queue_depth = 2;
  esp_err_t err = ESP_FAIL;
#if SOC_RMT_SUPPORT_DMA
  // DMA keeps the waveform clean while Wi-Fi interrupts are busy.
  cfg.mem_block_symbols = 1024;
  cfg.flags.with_dma = 1;
  err = rmt_new_tx_channel(&cfg, &channel_);
#endif
  for (size_t symbols : {192, 96, 48}) {
    if (err == ESP_OK) break;
    cfg.flags.with_dma = 0;
    cfg.mem_block_symbols = symbols;
    err = rmt_new_tx_channel(&cfg, &channel_);
  }
  if (err != ESP_OK) {
    channel_ = nullptr;
    setPower(false);
    parkDataPin();
    return false;
  }

  // WS2812B bit timing (T0H 0.3 us / T0L 0.9 us, T1H 0.9 us / T1L 0.3 us), MSB first.
  rmt_bytes_encoder_config_t enc = {};
  enc.bit0 = symbol(3, 9);
  enc.bit1 = symbol(9, 3);
  enc.flags.msb_first = 1;
  if (rmt_new_bytes_encoder(&enc, &encoder_) != ESP_OK || rmt_enable(channel_) != ESP_OK) {
    if (encoder_ != nullptr) rmt_del_encoder(encoder_);
    rmt_del_channel(channel_);
    encoder_ = nullptr;
    channel_ = nullptr;
    setPower(false);
    parkDataPin();
    return false;
  }
  buffer_.assign(ledCount_ * 3, 0);
  on_ = true;
  show(buffer_.data(), buffer_.size());  // start dark: some clones power up lit
  return true;
}

void LedStrip::powerOff() {
  if (on_) {
    rmt_tx_wait_all_done(channel_, kTxTimeoutMs);
    rmt_disable(channel_);
    rmt_del_channel(channel_);
    rmt_del_encoder(encoder_);
    channel_ = nullptr;
    encoder_ = nullptr;
    on_ = false;
  }
  parkDataPin();
  setPower(false);
}

bool LedStrip::show(const uint8_t* data, size_t len) {
  if (!on_) return false;
  if (rmt_tx_wait_all_done(channel_, kTxTimeoutMs) != ESP_OK) return false;
  delayMicroseconds(kLatchUs);
  if (len > buffer_.size()) len = buffer_.size();
  if (data != buffer_.data()) std::memcpy(buffer_.data(), data, len);
  rmt_transmit_config_t tx = {};
  tx.loop_count = 0;
  return rmt_transmit(channel_, encoder_, buffer_.data(), len, &tx) == ESP_OK;
}
