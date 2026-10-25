# Augen – animierte Augen für den 3D-Kopf

ESP32-C3 Super Mini + 2× GC9A01 (1,28", 240×240, rund). Die Augen werden auf dem
Gerät berechnet (Mensch, Katze, Roboter), gesteuert per Web-Oberfläche oder HTTP.
Keine externen Bibliotheken nötig.

## Verdrahtung

Grafischer Plan: [Verkabelung.html](Verkabelung.html)

| Display-Pin | ESP32-C3     | Hinweis                          |
|-------------|--------------|----------------------------------|
| VCC         | 3V3          | beide Displays                   |
| GND         | GND          | beide Displays                   |
| SCL         | GPIO 6       | beide Displays (SPI-Takt)        |
| SDA         | GPIO 7       | beide Displays (SPI-Daten)       |
| DC          | GPIO 4       | beide Displays                   |
| RST         | GPIO 5       | beide Displays                   |
| CS          | GPIO 10      | **linkes** Auge (von vorn)       |
| CS          | GPIO 3       | **rechtes** Auge                 |

GPIO 2, 8 und 9 bleiben frei (Boot-Pins / LED / BOOT-Taste). Pins ändern: oben in `Augen/Augen.ino`.

## Aufspielen

Arduino IDE, ESP32-Core 3.x, Board **ESP32C3 Dev Module**, *USB CDC On Boot: Enabled*.
Ordner `Augen/` öffnen, hochladen.

- **Farben vertauscht** (Rot ↔ Blau): `MADCTL_L/R` von `0x08` auf `0x00`.
- **Bild gedreht** (Einbaulage im Kopf): `MADCTL_L/R` = `0x68` (90°), `0xC8` (180°), `0xA8` (270°).
- **Bildstörungen** auf dem Steckbrett: kürzere Kabel oder `SPI_HZ` auf 20 MHz. Fest verlötet gehen meist 80 MHz (flüssiger).

## Erster Start

1. Ohne gespeichertes WLAN öffnet das Gerät **„Augen-Setup“** (Passwort `augen1234`).
2. Verbinden → Seite öffnet sich (sonst `http://192.168.4.1`) → *WLAN einrichten* → speichern.
3. Danach: **http://augen.local**. Ist das Heimnetz 20 s nicht erreichbar, kommt das Setup-WLAN wieder dazu.

## HTTP-Befehle

Alle per GET oder POST, Antwort ist der Zustand als JSON. CORS ist offen.

| Befehl | Werte |
|---|---|
| `/api/look?x=0.5&y=-0.2` | Blick, −1 … 1 (x rechts, y unten); pausiert die Automatik 4 s |
| `/api/expr?name=froh` | `neutral`, `froh`, `boese`, `traurig`, `ueberrascht`, `muede`, `skeptisch`, `verliebt`, `schlaf` |
| `/api/action?name=blinzeln` | `blinzeln`, `zwinkern_l`, `zwinkern_r`, `zu`, `auf`, `rollen`, `umschauen`, `schielen`, `erschrecken` |
| `/api/auto?on=1` | Automatik (Umherschauen + Blinzeln) an/aus |
| `/api/style?name=katze&iris=ffc414&lid=0a0a0e` | Stil `mensch`/`katze`/`roboter`, Farben `RRGGBB` (bleibt gespeichert) |
| `/api/state` | Zustand abfragen |
| `/api/presence?on=1` | Bewegung/Präsenz: weckt, startet Schlaf-Timer neu (`on=0` = schlafen legen) |
| `/api/sleep?nach=300&modus=aus` | von selbst einschlafen nach Sekunden (`0` = nie); `modus=zu` (Lider zu) oder `aus` (Display aus) |
| `/api/open`, `/api/close`, `/api/status` | Kontrakt des NexusHomeBase-Treibers `esp`: auf = wach, zu = schlafen |

Look, Ausdruck und Aktionen halten die Augen ebenfalls wach.

## Anbindung an NexusHomeBase

**Aktiv: PIR am Wandpanel-Pi (`nexus-pi`).** Der Bewegungsmelder hängt direkt am Pi und ist in
NexusHomeBase kein Gerät. Darum weckt das Pi-Skript `pir-display.sh` die Augen selbst: Bewegung →
`/api/presence?on=1` (höchstens alle 30 s), Display aus → `on=0`. Adresse in `/etc/nexus-panel/env`:
`AUGEN_URL=http://augen.fritz.box`. Code: NexusHomeBase `deploy/pi/`.

Für Bewegungsmelder, die NexusHomeBase als Gerät kennt (z. B. Aqara), geht es über den Treiber `esp`:

Ohne Code-Änderung in NexusHomeBase, über den vorhandenen Treiber **`esp`**:

1. Im Router eine **feste IP** für die Augen reservieren (Container lösen `augen.local` meist nicht auf).
2. In NexusHomeBase ein Gerät anlegen: Treiber `esp`, Config `{"host":"<IP der Augen>","kind":"cover"}`.
   Es erscheint als „Cover“: **auf = wach, zu = schlafen**.
3. Regel: Auslöser Bewegungsmelder → Augen **an** mit Dauer (z. B. 300 s). Nach Ablauf schaltet die
   Regel zu, die Augen schlafen ein. Neue Bewegung weckt sie wieder.

Alternativ ohne Regel-Dauer: in der Oberfläche „Von selbst einschlafen“ setzen; dann reicht es, bei
jeder Bewegung `/api/presence?on=1` (bzw. `/api/open`) aufzurufen.

Beispiel Home Assistant (`configuration.yaml`):

```yaml
rest_command:
  augen_ausdruck:
    url: "http://augen.local/api/expr?name={{ name }}"
  augen_aktion:
    url: "http://augen.local/api/action?name={{ name }}"
```

## Aufbau

- `Augen/augen_render.h` – zeichnet ein Auge zeilenweise (reines C++)
- `Augen/augen_anim.h` – Ausdrücke, Aktionen, Blinzeln, Automatik
- `Augen/gc9a01_bus.h` – beide Displays an einem SPI-Bus, DMA
- `Augen/web_ui.h` – Web-Oberfläche mit Live-Vorschau (gleiche Logik in JavaScript)
- `tools/host_test.cpp` – rendert Testbilder am Rechner:
  `c++ -std=c++17 -O2 -I Augen tools/host_test.cpp -o augen_test && ./augen_test ./`

Ändert man Aussehen oder Ausdrücke, beide Seiten anpassen: C++ (`augen_*.h`) und
JavaScript (`web_ui.h`), sonst weicht die Vorschau ab.
