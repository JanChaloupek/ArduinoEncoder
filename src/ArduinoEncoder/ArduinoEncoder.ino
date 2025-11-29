#include <Wire.h>
#include <EEPROM.h>

#define I2C_ADDR 0x08   // I2C adresa

// --- Počítadla ---
volatile long forwardM1 = 0, backwardM1 = 0;
volatile long forwardM2 = 0, backwardM2 = 0;

// --- Směr ---
volatile int8_t dirM1 = 0, dirM2 = 0;

// --- Čas posledního pulzu ---
unsigned long lastPulseM1 = 0, lastPulseM2 = 0;

// --- Flagy pro ISR ---
volatile bool pulseM1Flag = false;
volatile bool pulseM2Flag = false;

// --- Timeout pro reset směru ---
unsigned int DIR_TIMEOUT = 200; // default
const int EEPROM_ADDR_TIMEOUT = 0;

byte lastCommand = 0;
unsigned int pendingTimeout = 0;   // hodnota čekající na zápis
bool writeTimeoutFlag = false;     // flag pro zápis do EEPROM

// --- ISR M1 ---
void isrM1() {
  int A = (PIND & (1 << PIND2)) >> PIND2;
  int B = (PIND & (1 << PIND3)) >> PIND3;
  if (A == B) { forwardM1++; dirM1 = +1; }
  else { backwardM1++; dirM1 = -1; }
  pulseM1Flag = true; // jen flag
}

// --- ISR M2 ---
void isrM2() {
  int A = (PIND & (1 << PIND4)) >> PIND4;
  int B = (PIND & (1 << PIND5)) >> PIND5;
  if (A == B) { forwardM2++; dirM2 = +1; }
  else { backwardM2++; dirM2 = -1; }
  pulseM2Flag = true; // jen flag
}

void setup() {
  Wire.begin(I2C_ADDR);
  Wire.setClock(400000); // I2C Fast Mode
  Wire.onRequest(onRequest);
  Wire.onReceive(onReceive);

  pinMode(2, INPUT); pinMode(3, INPUT);
  pinMode(4, INPUT); pinMode(5, INPUT);

  attachInterrupt(digitalPinToInterrupt(2), isrM1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(4), isrM2, CHANGE);

  // načtení timeoutu z EEPROM
  unsigned int storedTimeout;
  EEPROM.get(EEPROM_ADDR_TIMEOUT, storedTimeout);
  if (storedTimeout > 0 && storedTimeout < 60000) {
    DIR_TIMEOUT = storedTimeout;
  }
}

void loop() {
  unsigned long now = millis();

  // zpracování flagů mimo ISR
  if (pulseM1Flag) {
    lastPulseM1 = now;
    pulseM1Flag = false;
  }
  if (pulseM2Flag) {
    lastPulseM2 = now;
    pulseM2Flag = false;
  }

  // reset směru po timeoutu
  if (now - lastPulseM1 > DIR_TIMEOUT) dirM1 = 0;
  if (now - lastPulseM2 > DIR_TIMEOUT) dirM2 = 0;

  // bezpečný zápis do EEPROM jen když oba motory stojí
  if (writeTimeoutFlag && dirM1 == 0 && dirM2 == 0) {
    EEPROM.put(EEPROM_ADDR_TIMEOUT, pendingTimeout);
    writeTimeoutFlag = false;
  }
}

void onReceive(int howMany) {
  if (howMany >= 1) {
    byte cmd = Wire.read();

    if (cmd == 0x02 && howMany >= 3) {
      // nastavení nové hodnoty timeoutu
      byte lo = Wire.read();
      byte hi = Wire.read();
      unsigned int newTimeout = (hi << 8) | lo;
      DIR_TIMEOUT = newTimeout;
      pendingTimeout = newTimeout;
      writeTimeoutFlag = true; // zápis odložen do loop()
    }

    lastCommand = cmd;
  }
}

void onRequest() {
  switch (lastCommand) {
    case 0x01: // reset
      forwardM1 = backwardM1 = 0;
      forwardM2 = backwardM2 = 0;
      dirM1 = dirM2 = 0;
      lastPulseM1 = lastPulseM2 = 0;
      Wire.write("OK");
      break;

    case 0x02: // vyčtení timeoutu
      Wire.write((byte*)&DIR_TIMEOUT, sizeof(DIR_TIMEOUT));
      break;

    case 0x11: { // M1 data
      Wire.write((byte*)&forwardM1, sizeof(forwardM1));
      Wire.write((byte*)&backwardM1, sizeof(backwardM1));
      Wire.write((byte*)&dirM1, sizeof(dirM1));
      Wire.write((byte*)&lastPulseM1, sizeof(lastPulseM1));
      break;
    }

    case 0x21: { // M2 data
      Wire.write((byte*)&forwardM2, sizeof(forwardM2));
      Wire.write((byte*)&backwardM2, sizeof(backwardM2));
      Wire.write((byte*)&dirM2, sizeof(dirM2));
      Wire.write((byte*)&lastPulseM2, sizeof(lastPulseM2));
      break;
    }

    default:
      Wire.write("ERR");
      break;
  }
}
