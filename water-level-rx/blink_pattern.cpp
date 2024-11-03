#include "blink_pattern.h"

bool BlinkPattern::isOn() {
  return this->state == BlinkState::On;
}

void BlinkPattern::setPattern(uint16_t onSpan, uint16_t offSpan) {
  this->onSpan = onSpan;
  this->offSpan = offSpan;
  this->reset();
}

void BlinkPattern::reset() {
  this->state = BlinkState::None;
}

bool BlinkPattern::update(uint32_t time) {
  switch (this->state) {
    case BlinkState::None:
      this->state = BlinkState::On;
      this->lastSwitchTime = time;
      return true;
    case BlinkState::On:
      if (this->offSpan > 0 && time - this->lastSwitchTime >= this->onSpan) {
        this->state = BlinkState::Off;
        this->lastSwitchTime = time;
        return true;
      }
      break;
    case BlinkState::Off:
      if (this->onSpan > 0 && time - this->lastSwitchTime >= this->offSpan) {
        this->state = BlinkState::On;
        this->lastSwitchTime = time;
        return true;
      }
      break;
  }
  return false;
}
