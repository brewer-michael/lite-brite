#pragma once

#include <driver/rmt_tx.h>

#include <cstddef>
#include <cstdint>
#include <vector>

// A WS2812B chain driven by the RMT peripheral, behind a switched supply.
//
// Each WS2812B draws ~0.7 mA even when dark, so a 256-LED panel left powered
// would drain the battery in days. The chain is only powered while a message
// is on screen; the rest of the time the switch is off and the data line is
// held low so it can't back-power the first LED.
class LedStrip {
 public:
  LedStrip(int dataPin, int powerPin, bool powerActiveHigh, size_t ledCount);

  // Parks the pins in the "off" state. Call once at boot.
  void begin();

  // Switches the supply on and starts the RMT channel. Returns false if the
  // RMT channel couldn't be created.
  bool powerOn();

  // Stops RMT, holds the data line low and switches the supply off.
  void powerOff();

  // Latches the supply switch off and the data line low through deep sleep
  // (belt and braces for the switch's pull resistor, and no stray pull-up on
  // the data line into the unpowered panel). begin() releases the latches.
  void holdOffDuringSleep();

  bool isOn() const { return on_; }

  // Sends a frame (3 bytes per LED, already in chain and color order). Waits
  // for the previous frame and the WS2812 latch time first.
  bool show(const uint8_t* data, size_t len);

 private:
  void setPower(bool on);
  void parkDataPin();

  int dataPin_;
  int powerPin_;
  bool powerActiveHigh_;
  size_t ledCount_;
  bool on_ = false;
  rmt_channel_handle_t channel_ = nullptr;
  rmt_encoder_handle_t encoder_ = nullptr;
  std::vector<uint8_t> buffer_;  // RMT reads this while transmitting
};
