// Animierte Augen: ESP32-C3 + 2x GC9A01 (1,28", 240x240), Steuerung per Web/HTTP.
// Board: "ESP32C3 Dev Module", USB CDC On Boot: Enabled. Keine externen Bibliotheken.
//
// Versionen:
//   1.0.0  Augen, Ausdrücke, Aktionen, Web-Oberfläche
//   1.1.0  Bewegungsmelder: Schlaf-Timer, Display aus, NexusHomeBase-Kontrakt
#define AUGEN_VERSION "1.1.0"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <DNSServer.h>
#include "gc9a01_bus.h"
#include "augen_render.h"
#include "augen_anim.h"
#include "web_ui.h"

// ---- Verdrahtung (ESP32-C3 Super Mini) ----
// SCK/MOSI auf den IOMUX-Pins des SPI2 (GPIO6/7), dann sind auch 80 MHz möglich.
constexpr int PIN_SCK = 6;     // Display "SCL"
constexpr int PIN_MOSI = 7;    // Display "SDA"
constexpr int PIN_DC = 4;      // beide Displays
constexpr int PIN_RST = 5;     // beide Displays
constexpr int PIN_CS_L = 10;   // linkes Auge (von vorn gesehen)
constexpr int PIN_CS_R = 3;    // rechtes Auge

constexpr uint32_t SPI_HZ = 40000000;  // auf dem Steckbrett 40 MHz, fest verlötet 80 MHz probieren
// MADCTL je Display: 0x08 = normal (BGR), 0x68 = 90°, 0xC8 = 180°, 0xA8 = 270°.
// Sind Rot und Blau vertauscht, das 0x08-Bit weglassen (0x00, 0x60, 0xC0, 0xA0).
constexpr uint8_t MADCTL_L = 0x08;
constexpr uint8_t MADCTL_R = 0x08;

constexpr const char* HOSTNAME = "augen";               // -> http://augen.local
constexpr const char* AP_SSID = "Augen-Setup";
constexpr const char* AP_PASS = "augen1234";

constexpr int STRIP = 24;  // Zeilen je DMA-Paket (240 / 24 = 10 Pakete je Auge)

using namespace augen;

Gc9a01Bus lcd;
Renderer renderer;
Engine engine;
WebServer server(80);
Preferences prefs;
DNSServer dns;
uint16_t* buf[2];

struct Aussehen { uint8_t stil; uint32_t iris, lid; } look;
static const uint32_t STD_IRIS[] = {0x3C82C8, 0xFFC414, 0x00DCFF};
static const uint32_t STD_LID[] = {0x1E1614, 0x0A0A0E, 0x283038};
static const char* const STIL_NAMEN[] = {"mensch", "katze", "roboter"};

bool styleDirty = true;
EyeFrame zuletzt[2];
uint32_t wlanStart = 0;
bool apAktiv = false;

// Schlaf/Wach: von Bewegungsmeldern (z.B. NexusHomeBase) gesteuert
bool wach = true, displayAus = false;
uint32_t letzteAktivitaet = 0, ausUm = 0;
uint32_t schlafNachS = 0;  // 0 = nie von selbst einschlafen
uint8_t schlafModus = 0;   // 0 = Augen zu, 1 = Display aus

// ---------- Darstellung ----------
static bool anders(const EyeFrame& a, const EyeFrame& b) {
  const float* x = &a.lookX; const float* y = &b.lookX;
  for (size_t i = 0; i < sizeof(EyeFrame) / sizeof(float); i++)
    if (fabsf(x[i] - y[i]) > 0.003f) return true;
  return false;
}

static void zeichneAuge(int e, const EyeFrame& f) {
  renderer.beginFrame(f, e == 1);
  for (int y = 0, k = 0; y < H; y += STRIP, k ^= 1) {
    renderer.renderRows(buf[k], y, STRIP);  // rechnen, während der vorige Streifen per DMA läuft
    lcd.sendStrip(e, y, STRIP, buf[k]);
  }
  lcd.finish();
}

static void zeichne(bool immer) {
  for (int e = 0; e < 2; e++) {
    EyeFrame f = engine.frame(e);
    if (immer || anders(f, zuletzt[e])) { zeichneAuge(e, f); zuletzt[e] = f; }
  }
}

static void wendeAussehenAn() {
  renderer.setStyle((Stil)look.stil, look.iris, look.lid);
  styleDirty = true;
}

// ---------- Schlafen / Wecken ----------
static void wecken() {
  letzteAktivitaet = millis();
  ausUm = 0;
  if (displayAus) { lcd.power(true); displayAus = false; styleDirty = true; }
  if (!wach) { wach = true; engine.aktion("aufwachen"); }
}

static void schlafen() {
  if (!wach) return;
  wach = false;
  engine.aktion("einschlafen");
  if (schlafModus == 1) ausUm = millis() + 1500;  // erst Lider schließen lassen
}

// Manche Befehle zählen als Aktivität: sie halten wach, ohne die Augen zu öffnen.
static void aktiv() {
  letzteAktivitaet = millis();
  if (displayAus) { lcd.power(true); displayAus = false; styleDirty = true; }
  ausUm = 0;
  wach = true;
}

static void pruefeSchlaf() {
  if (wach && schlafNachS && millis() - letzteAktivitaet > schlafNachS * 1000UL) schlafen();
  if (ausUm && (int32_t)(millis() - ausUm) >= 0) { ausUm = 0; lcd.power(false); displayAus = true; }
}

// ---------- Einstellungen ----------
static void ladeEinstellungen() {
  prefs.begin("augen", true);
  look.stil = prefs.getUChar("stil", MENSCH);
  if (look.stil > ROBOTER) look.stil = MENSCH;
  look.iris = prefs.getULong("iris", STD_IRIS[look.stil]);
  look.lid = prefs.getULong("lid", STD_LID[look.stil]);
  engine.setAuto(prefs.getBool("auto", true));
  schlafNachS = prefs.getULong("schlafS", 0);
  schlafModus = prefs.getUChar("schlafM", 0);
  prefs.end();
}

static void speichereAussehen() {
  prefs.begin("augen", false);
  prefs.putUChar("stil", look.stil);
  prefs.putULong("iris", look.iris);
  prefs.putULong("lid", look.lid);
  prefs.putBool("auto", engine.autoMode());
  prefs.putULong("schlafS", schlafNachS);
  prefs.putUChar("schlafM", schlafModus);
  prefs.end();
}

// ---------- HTTP ----------
static void sendeZustand() {
  char json[512];
  snprintf(json, sizeof(json),
           "{\"ausdruck\":\"%s\",\"auto\":%s,\"zu\":%s,\"x\":%.2f,\"y\":%.2f,"
           "\"stil\":\"%s\",\"iris\":\"%06lX\",\"lid\":\"%06lX\","
           "\"wach\":%s,\"schlaf_nach\":%lu,\"schlaf_modus\":\"%s\","
           "\"ip\":\"%s\",\"ssid\":\"%s\",\"ap\":%s,\"version\":\"%s\"}",
           engine.ausdruckName(), engine.autoMode() ? "true" : "false", engine.zu() ? "true" : "false",
           engine.blickX(), engine.blickY(), STIL_NAMEN[look.stil], (unsigned long)look.iris,
           (unsigned long)look.lid, wach ? "true" : "false", (unsigned long)schlafNachS,
           schlafModus ? "aus" : "zu", WiFi.localIP().toString().c_str(), WiFi.SSID().c_str(),
           apAktiv ? "true" : "false", AUGEN_VERSION);
  server.send(200, "application/json", json);
}

static void fehler(const char* text) {
  String s = String("{\"fehler\":\"") + text + "\"}";
  server.send(400, "application/json", s);
}

static bool farbe(const String& s, uint32_t& out) {
  String h = s;
  if (h.startsWith("#")) h.remove(0, 1);
  if (h.length() != 6) return false;
  char* end;
  uint32_t v = strtoul(h.c_str(), &end, 16);
  if (*end) return false;
  out = v;
  return true;
}

static void routen() {
  server.enableCORS(true);  // damit die Hausautomation-Oberfläche direkt aufrufen darf

  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); });
  server.on("/api/state", HTTP_ANY, sendeZustand);

  server.on("/api/look", HTTP_ANY, [] {
    if (!server.hasArg("x") && !server.hasArg("y")) return fehler("x und/oder y fehlt");
    aktiv();
    engine.look(server.arg("x").toFloat(), server.arg("y").toFloat());
    sendeZustand();
  });

  server.on("/api/expr", HTTP_ANY, [] {
    if (!engine.ausdruck(server.arg("name").c_str())) return fehler("unbekannter Ausdruck");
    aktiv();
    if (server.arg("name") == "schlaf") wach = false;
    sendeZustand();
  });

  server.on("/api/action", HTTP_ANY, [] {
    String n = server.arg("name");
    if (n == "aufwachen") { wecken(); return sendeZustand(); }
    if (n == "einschlafen") { schlafen(); return sendeZustand(); }
    if (!engine.aktion(n.c_str())) return fehler("unbekannte Aktion");
    aktiv();
    if (n == "zu") wach = false;
    sendeZustand();
  });

  server.on("/api/auto", HTTP_ANY, [] {
    String on = server.arg("on");
    engine.setAuto(on == "1" || on == "true" || on == "an");
    speichereAussehen();
    sendeZustand();
  });

  server.on("/api/style", HTTP_ANY, [] {
    if (server.hasArg("name")) {
      String n = server.arg("name");
      int s = -1;
      for (int i = 0; i < 3; i++) if (n == STIL_NAMEN[i]) s = i;
      if (s < 0) return fehler("unbekannter Stil");
      look.stil = s;
      look.iris = STD_IRIS[s];
      look.lid = STD_LID[s];
    }
    if (server.hasArg("iris") && !farbe(server.arg("iris"), look.iris)) return fehler("iris: Farbe als RRGGBB");
    if (server.hasArg("lid") && !farbe(server.arg("lid"), look.lid)) return fehler("lid: Farbe als RRGGBB");
    wendeAussehenAn();
    speichereAussehen();
    sendeZustand();
  });

  // Bewegungs-/Präsenzmelder: jede Meldung weckt und startet den Schlaf-Timer neu
  server.on("/api/presence", HTTP_ANY, [] {
    String on = server.arg("on");
    if (on == "0" || on == "false" || on == "aus") schlafen(); else wecken();
    sendeZustand();
  });

  server.on("/api/sleep", HTTP_ANY, [] {
    if (server.hasArg("nach")) schlafNachS = server.arg("nach").toInt();
    if (server.hasArg("modus")) schlafModus = server.arg("modus") == "aus" ? 1 : 0;
    speichereAussehen();
    sendeZustand();
  });

  // Kontrakt des NexusHomeBase-Treibers "esp" (kind: cover): auf = wach, zu = schlafen
  server.on("/api/open", HTTP_ANY, [] { wecken(); server.send(200, "application/json", "{\"state\":\"open\"}"); });
  server.on("/api/close", HTTP_ANY, [] { schlafen(); server.send(200, "application/json", "{\"state\":\"closed\"}"); });
  server.on("/api/stop", HTTP_ANY, [] { server.send(200, "application/json", wach ? "{\"state\":\"open\"}" : "{\"state\":\"closed\"}"); });
  server.on("/api/status", HTTP_ANY, [] { server.send(200, "application/json", wach ? "{\"state\":\"open\"}" : "{\"state\":\"closed\"}"); });

  server.on("/api/wifi", HTTP_POST, [] {
    if (!server.hasArg("ssid") || server.arg("ssid").isEmpty()) return fehler("ssid fehlt");
    prefs.begin("augen", false);
    prefs.putString("ssid", server.arg("ssid"));
    prefs.putString("pass", server.arg("pass"));
    prefs.end();
    server.send(200, "application/json", "{\"ok\":true,\"hinweis\":\"Neustart, danach unter http://augen.local\"}");
    delay(500);
    ESP.restart();
  });

  server.onNotFound([] {
    if (apAktiv) {  // Captive Portal: alles auf die Startseite
      server.sendHeader("Location", "http://192.168.4.1/");
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "nicht gefunden");
    }
  });
}

// ---------- WLAN ----------
static void starteAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  dns.start(53, "*", WiFi.softAPIP());
  apAktiv = true;
  Serial.printf("Setup-WLAN \"%s\" (Passwort %s) -> http://192.168.4.1\n", AP_SSID, AP_PASS);
}

static void starteWLAN() {
  prefs.begin("augen", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();
  WiFi.setHostname(HOSTNAME);
  if (ssid.isEmpty()) { starteAP(); return; }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // kürzere Antwortzeiten
  WiFi.begin(ssid.c_str(), pass.c_str());
  wlanStart = millis();
}

static void pruefeWLAN() {
  static bool gemeldet = false;
  if (WiFi.status() == WL_CONNECTED) {
    if (!gemeldet) {
      gemeldet = true;
      MDNS.begin(HOSTNAME);
      MDNS.addService("http", "tcp", 80);
      Serial.printf("Verbunden: http://%s.local  (%s)\n", HOSTNAME, WiFi.localIP().toString().c_str());
      if (apAktiv) { dns.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); apAktiv = false; }
    }
  } else if (!apAktiv && wlanStart && millis() - wlanStart > 20000) {
    starteAP();  // Heimnetz nicht erreichbar: Setup-WLAN zusätzlich öffnen
  }
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(115200);
  Serial.println("Augen " AUGEN_VERSION);
  size_t bytes = W * STRIP * sizeof(uint16_t);
  if (!lcd.begin(PIN_SCK, PIN_MOSI, PIN_DC, PIN_RST, bytes) ||
      !lcd.addPanel(PIN_CS_L, SPI_HZ, MADCTL_L) || !lcd.addPanel(PIN_CS_R, SPI_HZ, MADCTL_R)) {
    Serial.println("SPI-Start fehlgeschlagen");
  }
  buf[0] = Gc9a01Bus::allocBuffer(bytes);
  buf[1] = Gc9a01Bus::allocBuffer(bytes);
  if (!buf[0] || !buf[1] || !renderer.begin()) Serial.println("Zu wenig Speicher");

  engine.begin(esp_random());
  ladeEinstellungen();
  letzteAktivitaet = millis();
  wendeAussehenAn();

  starteWLAN();
  routen();
  server.begin();
}

void loop() {
  static uint32_t vorher = millis();
  server.handleClient();
  if (apAktiv) dns.processNextRequest();
  pruefeWLAN();
  pruefeSchlaf();

  uint32_t jetzt = millis();
  engine.update((jetzt - vorher) / 1000.f);
  vorher = jetzt;

  if (!displayAus) zeichne(styleDirty);
  styleDirty = false;
  delay(1);
}
