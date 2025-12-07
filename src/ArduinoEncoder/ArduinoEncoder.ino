/*
  Projekt: Dva enkodéry + dva TM1638 panely + I2C + EEPROM + Serial 115200
  Autor: Jan Chaloupek (s podporou Copilot)
  Datum: 7.12.2025

  Přehled:
  - Čte dva rotační enkodéry (levý na D2/D4, pravý na D3/D5).
  - Zobrazuje hodnoty na dvou TM1638 panelech (levý: D6/D7/D8, pravý: D9/D10/D11).
  - Každý panel ukazuje znak panelu (L/r), režim (d/f/b) a 6cifernou hodnotu.
  - Tlačítka se čtou střídavě (levý, pravý) s intervalem 800 ms, aby se minimalizoval zásah do ISR.
  - I2C slave na adrese 0x08: reset, nastavení timeoutu, čtení stavů enkodérů.
  - Timeout (DIR_TIMEOUT) se ukládá do EEPROM.
  - Serial běží na 115200 baud pro rychlé diagnostické výpisy.

  Ovládání tlačítek TM1638:
  - S1: znovu inicializuje oba displeje (begin), užitečné po „zaseknutí“ DIO.
  - S2: přepíná režim zobrazení (d → f → b → d).
  - S3: resetuje oba enkodéry a displeje (reset).

  Režimy:
  - d: difference (forward - backward)
  - f: forward (jen dopředu)
  - b: backward (jen dozadu)

  Doporučení zapojení:
  - Vyhni se D0/D1 (UART) a D13 (LED/SPI) pro obousměrné linky DIO.
  - I2C je pevně na A4 (SDA) a A5 (SCL).
*/

#include <Wire.h>
#include <EEPROM.h>
#include <TM1638plus.h>

// Vlastní třídy
#include "Encoder.h"
#include "DisplayController.h"

// --- I2C adresa zařízení ---
#define I2C_ADDR 0x08

// --- TM1638 piny (za sebou D6–D11) ---
#define STB_LEFT 6
#define CLK_LEFT 7
#define DIO_LEFT 8

#define STB_RIGHT 9
#define CLK_RIGHT 10
#define DIO_RIGHT 11

// ==============================
// Globální instance tříd
// ==============================
Encoder LeftEncoder(2, 4);   // Levý enkodér: A=D2 (HW INT0), B=D4
Encoder RightEncoder(3, 5);  // Pravý enkodér: A=D3 (HW INT1), B=D5

DisplayController leftDisplay(STB_LEFT, CLK_LEFT, DIO_LEFT, 'L');
DisplayController rightDisplay(STB_RIGHT, CLK_RIGHT, DIO_RIGHT, 'r');

// ==============================
// ISR funkce (deklarace)
// ==============================
void isrLeftA();   // ISR pro levý enkodér (kanál A)
void isrRightA();  // ISR pro pravý enkodér (kanál A)

// ==============================
// Stav aplikace a časování
// ==============================
unsigned int DIR_TIMEOUT = 200;            // Timeout pro směr (ms)
const int EEPROM_ADDR_TIMEOUT = 0;         // Adresa EEPROM pro uložení timeoutu
byte lastCommand = 0;                       // Poslední I2C příkaz (pro onRequest)
unsigned int pendingTimeout = 0;            // Hodnota timeoutu čekající na zápis
bool writeTimeoutFlag = false;              // Příznak pro zápis do EEPROM, až když enkodéry stojí

int displayMode = 0;                        // 0=d, 1=f, 2=b (společný režim obou panelů)
const unsigned long DISP_UPDATE_MS = 200;   // Interval aktualizace displejů
const unsigned long BUTTON_POLL_MS = 800;   // Interval střídavého čtení tlačítek

unsigned long lastUpdate = 0;
unsigned long lastButtonPoll = 0;
bool pollLeftPanel = true;                  // true=číst levý, false=číst pravý

// ==============================
// Prototypy I2C handlerů
// ==============================
void onReceive(int howMany);
void onRequest();

// ==============================
// Setup
// ==============================
void setup() {
  Serial.begin(115200); // Rychlé diagnostické výpisy

  // Inicializace I2C jako slave
  Wire.begin(I2C_ADDR);
  Wire.setClock(400000); // I2C Fast Mode (400 kHz)
  Wire.onRequest(onRequest);
  Wire.onReceive(onReceive);

  // Přerušení pro enkodéry
  attachInterrupt(digitalPinToInterrupt(2), isrLeftA, RISING);   // Levý enkodér A na D2 (INT0)
  attachInterrupt(digitalPinToInterrupt(3), isrRightA, RISING);  // Pravý enkodér A na D3 (INT1)

  // Načtení timeoutu z EEPROM (validace rozumného rozsahu)
  unsigned int storedTimeout;
  EEPROM.get(EEPROM_ADDR_TIMEOUT, storedTimeout);
  if (storedTimeout > 0 && storedTimeout < 60000) {
    DIR_TIMEOUT = storedTimeout;
  }

  // Inicializace panelů
  leftDisplay.begin();
  rightDisplay.begin();

  Serial.println("Start: piny D2–D11 za sebou, Serial 115200, I2C 0x08");
}

// ==============================
// Loop
// ==============================
void loop() {
  unsigned long now = millis();

  // Vynulování směru po timeoutu (prevence „drženého“ směru bez pulzů)
  if (now - LeftEncoder.lastPulse > DIR_TIMEOUT)  LeftEncoder.dir = 0;
  if (now - RightEncoder.lastPulse > DIR_TIMEOUT) RightEncoder.dir = 0;

  // Odložený zápis timeoutu do EEPROM (jen když enkodéry stojí)
  if (writeTimeoutFlag && LeftEncoder.dir == 0 && RightEncoder.dir == 0) {
    EEPROM.put(EEPROM_ADDR_TIMEOUT, pendingTimeout);
    writeTimeoutFlag = false;
    Serial.print("EEPROM ulozen timeout: ");
    Serial.println(pendingTimeout);
  }

  // Střídavé čtení tlačítek (levý/pravý panel) v delším intervalu,
  // aby bit-banging TM1638 nezasahoval do ISR počítání pulzů.
  if (now - lastButtonPoll > BUTTON_POLL_MS) {
    uint8_t buttons = pollLeftPanel ? leftDisplay.readButtons()
                                    : rightDisplay.readButtons();

    // S1: znovu inicializace obou displejů (užitečné při „zaseknutí“ DIO)
    if (buttons & 0x01) {
      leftDisplay.begin();
      rightDisplay.begin();
      Serial.println("S1: Displeje znovu inicializovany");
    }

    // S2: přepnutí režimu zobrazení (d/f/b)
    if (buttons & 0x02) {
      displayMode = (displayMode + 1) % 3;
      Serial.print("S2: Rezim zmenen na: ");
      Serial.println(displayMode);
    }

    // S3: reset enkodérů a displejů
    if (buttons & 0x04) {
      LeftEncoder.reset();
      RightEncoder.reset();
      leftDisplay.reset();
      rightDisplay.reset();
      Serial.println("S3: Reset enkoderu a displeju");
    }

    pollLeftPanel = !pollLeftPanel;
    lastButtonPoll = now;
  }

  // Aktualizace displejů a diagnostický výpis na Serial
  if (now - lastUpdate > DISP_UPDATE_MS) {
    long leftVal  = LeftEncoder.getValue(displayMode);
    long rightVal = RightEncoder.getValue(displayMode);

    leftDisplay.renderValue(leftVal, displayMode);
    rightDisplay.renderValue(rightVal, displayMode);

    // Serial.print("Left: ");
    // Serial.print(leftVal);
    // Serial.print(" | Right: ");
    // Serial.print(rightVal);
    // Serial.print(" | Mode: ");
    // Serial.println(displayMode);

    lastUpdate = now;
  }
}

// ==============================
// ISR implementace
// ==============================

// ISR pro levý enkodér (kanál A)
// Rychlá obsluha: směr odvozen z pinB, přičte forward/backward, nastaví dir a lastPulse.
void isrLeftA() {
  LeftEncoder.handlePulseA();
}

// ISR pro pravý enkodér (kanál A) přes PCINT
void isrRightA() {
  RightEncoder.handlePulseA();
}

// ==============================
// I2C obsluha (slave)
// ==============================

// onReceive: přijímá příkazy od mastera.
// Protokol:
//  - 0x01: reset obou enkodérů (bez payloadu)
//  - 0x02: nastavení timeoutu (payload: 2 bajty, lo/hi)
//  - 0x11: požadavek států levého enkodéru (připraví data pro onRequest)
//  - 0x21: požadavek států pravého enkodéru (připraví data pro onRequest)
void onReceive(int howMany) {
  if (howMany >= 1) {
    byte cmd = Wire.read();
    lastCommand = cmd;

    if (cmd == 0x02 && howMany >= 3) {
      // Nastavení timeoutu: lo, hi
      byte lo = Wire.read();
      byte hi = Wire.read();
      unsigned int newTimeout = (hi << 8) | lo;
      DIR_TIMEOUT = newTimeout;
      pendingTimeout = newTimeout;
      writeTimeoutFlag = true;
      Serial.print("I2C: prijat timeout = ");
      Serial.println(newTimeout);
    } else if (cmd == 0x01) {
      // Reset přes I2C se provede v onRequest (když si master „vyžádá“ odpověď)
      Serial.println("I2C: prijat prikaz RESET");
    } else if (cmd == 0x11) {
      Serial.println("I2C: dotaz na LEFT data");
    } else if (cmd == 0x21) {
      Serial.println("I2C: dotaz na RIGHT data");
    } else {
      Serial.print("I2C: neznamy prikaz ");
      Serial.println(cmd);
    }
  }
}

// onRequest: master si vyžádá odpověď.
// Odpověď se řídí posledním přijatým příkazem (lastCommand).
void onRequest() {
  switch (lastCommand) {
    case 0x01: {
      // Reset obou enkodérů
      LeftEncoder.reset();
      RightEncoder.reset();
      Wire.write("OK"); // Krátká potvrzovací zpráva
      break;
    }
    case 0x02: {
      // Vrácení aktuálního timeoutu (2 bajty)
      Wire.write((byte*)&DIR_TIMEOUT, sizeof(DIR_TIMEOUT));
      break;
    }
    case 0x11: {
      // Stav levého enkodéru: forward, backward, dir, lastPulse
      Wire.write((byte*)&LeftEncoder.forward,   sizeof(LeftEncoder.forward));
      Wire.write((byte*)&LeftEncoder.backward,  sizeof(LeftEncoder.backward));
      Wire.write((byte*)&LeftEncoder.dir,       sizeof(LeftEncoder.dir));
      Wire.write((byte*)&LeftEncoder.lastPulse, sizeof(LeftEncoder.lastPulse));
      break;
    }
    case 0x21: {
      // Stav pravého enkodéru: forward, backward, dir, lastPulse
      Wire.write((byte*)&RightEncoder.forward,   sizeof(RightEncoder.forward));
      Wire.write((byte*)&RightEncoder.backward,  sizeof(RightEncoder.backward));
      Wire.write((byte*)&RightEncoder.dir,       sizeof(RightEncoder.dir));
      Wire.write((byte*)&RightEncoder.lastPulse, sizeof(RightEncoder.lastPulse));
      break;
    }
    default: {
      Wire.write("ERR"); // Neznámý příkaz
      break;
    }
  }
}
