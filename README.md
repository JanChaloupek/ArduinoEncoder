# ArduinoEncoder

Projekt pro čtení dvou rotačních enkodérů a zobrazení hodnot na dvou TM1638 panelech.  
Arduino Nano funguje zároveň jako I²C slave (adresa `0x08`) a poskytuje data o enkodérech.

---

## Zapojení pinů Arduino Nano

| Funkce            | Pin Arduino | Poznámka                  |
|-------------------|-------------|---------------------------|
| Levý enkodér A    | D2          | HW interrupt (ideální)    |
| Levý enkodér B    | D3          | HW interrupt (ideální)    |
| Pravý enkodér A   | D4          | PCINT                     |
| Pravý enkodér B   | D5          | PCINT                     |
| Levý panel STB    | D6          | Chip select               |
| Levý panel CLK    | D7          | Hodiny                    |
| Levý panel DIO    | D8          | Data (obousměrné)         |
| Pravý panel STB   | D9          | Chip select               |
| Pravý panel CLK   | D10         | Hodiny                    |
| Pravý panel DIO   | D11         | Data (obousměrné)         |
| I2C SDA           | A4          | Datová linka I2C          |
| I2C SCL           | A5          | Hodiny I2C                |

---

## Ovládání tlačítek na TM1638

Každý panel má vlastní sadu tlačítek. Čtení probíhá střídavě (levý/pravý panel) v intervalu 800 ms, aby nedocházelo ke ztrátě pulzů enkodérů.

- **S1** – znovu inicializuje oba displeje (`begin()`), užitečné při problémech s DIO.  
- **S2** – přepíná režim zobrazení (cyklicky):  
  - `d` = difference (forward - backward)  
  - `f` = forward (jen dopředu)  
  - `b` = backward (jen dozadu)  
- **S3** – resetuje oba enkodéry a displeje (`reset()`).

---

## Funkce displejů

Každý panel zobrazuje 8 znaků ve formátu:
```<id><mode><value>```

- **id** – identifikátor panelu (`L` pro levý, `r` pro pravý)  
- **mode** – aktuální režim (`d`, `f`, `b`)  
- **value** – 6ciferná hodnota podle zvoleného režimu  

Příklad:
- Levý panel: `Ld000123` → levý enkodér, režim difference, hodnota 123  
- Pravý panel: `rf000456` → pravý enkodér, režim forward, hodnota 456  

---

## I2C komunikace

Arduino Nano funguje jako I²C slave (adresa `0x08`) a poskytuje data z dvou enkodérů.  
Protokol je navržen tak, aby byl přehledný, rozšiřitelný a jednoznačný.

### Příkazy

| Příkaz | Hex  | Význam                                                   | Odpověď (formát)                                                                 |
|--------|------|----------------------------------------------------------|----------------------------------------------------------------------------------|
| Reset  | `0x01` | Vynuluje všechny čítače, směry a časové značky           | Řetězec `"OK"`                                                                   |
| Timeout| `0x02` | Nastavení nebo vyčtení timeoutu pro reset směru (EEPROM) | - Jen `0x02`: Arduino vrátí aktuální `DIR_TIMEOUT` (2 bajty, `uint16_t`)<br>- `0x02` + 2 bajty: Arduino nastaví novou hodnotu a uloží ji do EEPROM (zápis proběhne bezpečně jen když motory stojí) |
| M1 data| `0x11` | Čtení dat pro enkodér M1 (levý)             | Forward (4B, `long`)<br>Backward (4B, `long`)<br>Dir (1B, `int8_t`)<br>LastPulse (4B, `unsigned long`) |
| M2 data| `0x21` | Čtení dat pro enkodér M2 (pravý)                          | Forward (4B, `long`)<br>Backward (4B, `long`)<br>Dir (1B, `int8_t`)<br>LastPulse (4B, `unsigned long`) |

---

## Detaily polí

- **Forward / Backward**  
  Počítadla pulzů ve směru dopředu a dozadu. Typ `long` (4 bajty).

- **Dir**  
  Aktuální směr pohybu:  
  - `+1` = dopředně  
  - `-1` = zpětně  
  - `0` = stojí (automaticky resetováno po `DIR_TIMEOUT` ms bez pulsu)

- **LastPulse**  
  Čas posledního pulzu v milisekundách od startu (`millis()`).
  
- **DIR_TIMEOUT**  
  Hodnota v milisekundách, po které se směr resetuje na `0`, pokud nepřišel žádný puls.  
  Lze ji nastavit příkazem `0x02` a ukládá se do EEPROM.  
  Zápis do EEPROM probíhá bezpečně jen tehdy, když oba motory stojí, aby se neztratily pulsy.

---

## Poznámky k implementaci

- **ISR** jsou krátké: jen inkrementace čítačů a nastavení flagu.  
- **Časování** (`millis()`) se provádí v hlavní smyčce (`loop()`), ne v ISR.  
- **EEPROM zápis** je odložený a provádí se jen v klidovém stavu (oba motory stojí).  
- **I²C rychlost**: nastaveno na 400 kHz (`Wire.setClock(400000);`).  
- **Reset (`0x01`)**: vynuluje čítače, směry i časové značky.  
- **Displeje**: inicializace (`begin()`), reset (`reset()`), zobrazení hodnoty (`renderValue()`).

---

## Shrnutí

Projekt poskytuje:
- Stabilní čtení dvou enkodérů s minimálními ISR.  
- Zobrazení hodnot na dvou TM1638 panelech s režimem a identifikátorem.  
- Ovládání přes tlačítka (S1–S3).  
- I²C protokol pro integraci s dalšími systémy.  
- EEPROM pro trvalé uložení konfigurace timeoutu.  

Dokumentace je připravena jako README pro GitHub.

