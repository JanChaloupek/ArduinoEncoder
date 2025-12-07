/*
  DisplayController.cpp
  Implementace ovladače TM1638 panelu.
*/

#include "DisplayController.h"

DisplayController::DisplayController(uint8_t stb, uint8_t clk, uint8_t dio, char id, bool high_freq)
  : tm(stb, clk, dio, high_freq), idChar(id) {}

void DisplayController::begin() {
  tm.displayBegin();
  tm.reset();
  tm.brightness(2); // Jas 0–7; 2 je střídmá hodnota
}

void DisplayController::renderValue(long value, int mode) {
  char modeChar;
  if (mode == 0) modeChar = 'd';
  else if (mode == 1) modeChar = 'f';
  else modeChar = 'b';

  char buf[9]; // 8 znaků + '\0'
  snprintf(buf, sizeof(buf), "%c%c%06ld", idChar, modeChar, value);
  tm.displayText(buf);
}

uint8_t DisplayController::readButtons() {
  return tm.readButtons();
}

void DisplayController::reset() {
  tm.reset();
}
