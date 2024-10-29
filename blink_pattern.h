#ifndef _BLINK_PATTERN_H
#define _BLINK_PATTERN_H

#include <cstdint>

class BlinkPattern {
private:
  enum class BlinkState {
    None, On, Off,
  };

  uint16_t onSpan = 0;
  uint16_t offSpan = 0;
  BlinkState state = BlinkState::None;
  uint32_t nextSwitchTime = 0;

public:
  bool isOn();
  void setPattern(uint16_t onSpan, uint16_t offSpan);
  void reset();
  bool update(uint32_t time);
};

#endif
