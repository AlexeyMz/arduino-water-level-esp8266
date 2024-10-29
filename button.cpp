#include <Arduino.h>

#include "button.h"

Button::Button(uint8_t pin)
  : pin(pin) {}

bool Button::isPressed() {
  return this->pressed;
}

bool Button::isPressedFor(uint32_t time, uint32_t span) {
  return this->pressed && time >= span && this->lastToggleTime < (time - span);
}

bool Button::isReleasedFor(uint32_t time, uint32_t span) {
  return !this->pressed && time >= span && this->lastToggleTime < (time - span);
}

void Button::update(uint32_t time) {
  int pressed = digitalRead(this->pin);
  if (pressed > 0 && !this->pressed) {
    this->pressed = true;
    this->lastToggleTime = time;
  } else if (pressed == 0 && this->pressed) {
    this->pressed = false;
    this->lastToggleTime = time;
  }
}
