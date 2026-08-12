#pragma once

#include <HalDisplay.h>
#include <HalGPIO.h>

#include <cstdint>

// Network workflows replace large parts of the screen as their state changes.
// X4-class panels need a clean waveform for those transitions, but FULL_REFRESH
// uses the panel's multi-flash GC waveform. HALF_REFRESH provides the clean,
// single-pass transition used by the stock X4 firmware. Repeated renders within
// the same state (progress, signal strength, selection) remain fast.
class ScreenTransitionRefresh {
 public:
  HalDisplay::RefreshMode modeFor(const uint8_t screenState) {
    const bool screenChanged = !hasRendered_ || screenState != lastScreenState_;
    lastScreenState_ = screenState;
    hasRendered_ = true;
    return screenChanged && !gpio.deviceIsX3() ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH;
  }

 private:
  uint8_t lastScreenState_ = 0;
  bool hasRendered_ = false;
};
