/*
  Encoder.h
  Třída pro obsluhu rotačního enkodéru.
  - Počítá pulzy dopředu/dozadu, odvozuje směr z kanálu B.
  - Minimalistická ISR: rychlé větvení podle pinB, inkrementace a uložení času.
*/

#pragma once

#include <Arduino.h>

class Encoder {
public:
  volatile long forward = 0;     // Počet kroků dopředu
  volatile long backward = 0;    // Počet kroků dozadu
  volatile int8_t dir = 0;       // Směr (+1, -1, 0 pokud stojí)
  unsigned long lastPulse = 0;   // Čas posledního pulsu (millis)
  int pinA, pinB;                // Fyzické piny enkodéru

  // Konstruktor: nastaví piny jako vstupy
  Encoder(int a, int b);

  // ISR handler pro pulz na kanálu A
  void handlePulseA();

  // Reset čítačů a stavu
  void reset();

  // Vrátí hodnotu podle režimu (0=difference, 1=forward, 2=backward)
  long getValue(int mode) const;
};
