/*
  Encoder.cpp
  Implementace třídy Encoder.
*/

#include "Encoder.h"

Encoder::Encoder(int a, int b) : pinA(a), pinB(b) {
  pinMode(pinA, INPUT);
  pinMode(pinB, INPUT);
}

void Encoder::handlePulseA() {
  // Odvození směru: čteme kanál B
  if (digitalRead(pinB) == HIGH) {
    forward++;
    dir = +1;
  } else {
    backward++;
    dir = -1;
  }
  lastPulse = millis();
}

void Encoder::reset() {
  forward = 0;
  backward = 0;
  dir = 0;
  lastPulse = 0;
}

long Encoder::getValue(int mode) const {
  if (mode == 0) return forward - backward; // difference
  if (mode == 1) return forward;            // forward only
  if (mode == 2) return backward;           // backward only
  return 0;
}
