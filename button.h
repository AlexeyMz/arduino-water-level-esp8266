#ifndef _BUTTON_H
#define _BUTTON_H

#include <cstdint>

class Button {
  uint8_t pin;
  bool pressed = false;
  uint32_t lastToggleTime = 0;

public:
  Button(uint8_t pin);
  bool isPressed();
  bool isPressedFor(uint32_t time, uint32_t span);
  bool isReleasedFor(uint32_t time, uint32_t span);
  void update(uint32_t time);
};

#endif
