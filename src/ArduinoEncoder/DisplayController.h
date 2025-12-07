/*
  DisplayController.h
  Ovladač TM1638 panelu (TM1638plus).
  - Inicializace, zobrazení hodnoty s režimem, čtení tlačítek, reset.
  - Výstupní formát: "<id><mode><value>" např. "Ld000123".
*/

#pragma once

#include <Arduino.h>
#include <TM1638plus.h>

class DisplayController {
public:
  TM1638plus tm;
  const char idChar; // Identifikátor panelu (L/r)

  // Konstruktor: stb/clk/dio + znak panelu; high_freq=false
  DisplayController(uint8_t stb, uint8_t clk, uint8_t dio, char id, bool high_freq=false);

  // Inicializace panelu: zapnutí, reset, jas
  void begin();

  // Zobrazení hodnoty s režimem (mode: 0=d,1=f,2=b)
  void renderValue(long value, int mode);

  // Čtení tlačítek (bit maska dle knihovny)
  uint8_t readButtons();

  // Reset panelu (mazání displeje, reset stavu)
  void reset();
};
