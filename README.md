# ArduinoEncoder
Dokumentace I²C protokolu pro Arduino Nano (enkodéry)

## Přehled
Arduino Nano funguje jako I²C slave (adresa `0x08`) a poskytuje data z dvou enkodérů.  
Protokol je navržen tak, aby byl přehledný, rozšiřitelný a jednoznačný.

---

## Příkazy

| Příkaz | Hex  | Význam                                                   | Odpověď (formát)                                                                 |
|--------|------|----------------------------------------------------------|----------------------------------------------------------------------------------|
| Reset  | `0x01` | Vynuluje všechny čítače, směry a časové značky           | Řetězec `"OK"`                                                                   |
| Timeout| `0x02` | Nastavení nebo vyčtení timeoutu pro reset směru (EEPROM) | - Jen `0x02`: Arduino vrátí aktuální `DIR_TIMEOUT` (2 bajty, `uint16_t`)<br>- `0x02` + 2 bajty: Arduino nastaví novou hodnotu a uloží ji do EEPROM (zápis proběhne bezpečně jen když motory stojí) |
| M1 data| `0x11` | Čtení dat pro enkodér M1                                 | Forward (4B, `long`)<br>Backward (4B, `long`)<br>Dir (1B, `int8_t`)<br>LastPulse (4B, `unsigned long`) |
| M2 data| `0x21` | Čtení dat pro enkodér M2                                 | Forward (4B, `long`)<br>Backward (4B, `long`)<br>Dir (1B, `int8_t`)<br>LastPulse (4B, `unsigned long`) |

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
  Hodnota se ukládá mimo ISR, aby přerušení byla co nejrychlejší.

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

---

## Shrnutí
Protokol je kompaktní, jednoznačný a bezpečný:  
- Servisní příkazy (`0x01`, `0x02`) jsou na začátku.  
- Datové příkazy (`0x11`, `0x21`) poskytují kompletní informace o enkodérech.  
- Směr se resetuje automaticky po nastaveném timeoutu.  
- EEPROM se používá jen pro konfiguraci, nikdy pro rychlé logování.
