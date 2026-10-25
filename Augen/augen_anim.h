#pragma once
// Animations-Engine: Ausdrücke, Aktionen, Blinzeln und Automatik ("Leben").
// Befehle setzen nur Ziele; update() führt die Augen weich dorthin.
// Die Web-Oberfläche enthält dieselbe Logik in JavaScript für die Vorschau.
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "augen_render.h"

namespace augen {

struct Lider { float lidU, lidL, curvL, tiltU, tiltL, pupil; };
struct Ausdruck { const char* name; Lider l, r; };

static const Ausdruck AUSDRUECKE[] = {
  {"neutral",     {0.18f, 0.10f, 0, 0, 0, 0.50f},          {0.18f, 0.10f, 0, 0, 0, 0.50f}},
  {"froh",        {0.12f, 0.72f, 1, 0, 0, 0.55f},          {0.12f, 0.72f, 1, 0, 0, 0.55f}},
  {"boese",       {0.30f, 0.22f, 0, 0.75f, -0.2f, 0.30f},  {0.30f, 0.22f, 0, 0.75f, -0.2f, 0.30f}},
  {"traurig",     {0.32f, 0.08f, 0, -0.75f, 0, 0.65f},     {0.32f, 0.08f, 0, -0.75f, 0, 0.65f}},
  {"ueberrascht", {0.00f, 0.00f, 0, 0, 0, 0.12f},          {0.00f, 0.00f, 0, 0, 0, 0.12f}},
  {"muede",       {0.58f, 0.12f, 0, -0.15f, 0, 0.60f},     {0.58f, 0.12f, 0, -0.15f, 0, 0.60f}},
  {"skeptisch",   {0.48f, 0.32f, 0, 0.20f, 0, 0.45f},      {0.08f, 0.05f, 0, -0.25f, 0, 0.45f}},
  {"verliebt",    {0.15f, 0.55f, 0.8f, -0.30f, 0, 1.00f},  {0.15f, 0.55f, 0.8f, -0.30f, 0, 1.00f}},
  {"schlaf",      {1.00f, 0.15f, 0, 0, 0, 0.50f},          {1.00f, 0.15f, 0, 0, 0, 0.50f}},
};
constexpr int ANZ_AUSDRUECKE = sizeof(AUSDRUECKE) / sizeof(AUSDRUECKE[0]);
constexpr int UEBERRASCHT = 4;

class Engine {
 public:
  void begin(uint32_t seed) {
    rs = seed ? seed : 0x2545F491u;
    ausdruck("neutral");
    for (int e = 0; e < 2; e++) {
      const Lider& z = ziel[e];
      cur[e].lidU = z.lidU; cur[e].lidL = z.lidL; cur[e].curvL = z.curvL;
      cur[e].tiltU = z.tiltU; cur[e].tiltL = z.tiltL; cur[e].pupil = z.pupil;
    }
  }

  void update(float dt) {
    if (dt > 0.2f) dt = 0.2f;
    t += dt;
    if (seq) runSeq(dt);

    if (autoOn && !closed && !seq && t > manualUntil) {
      if (t >= nextSacc) {
        if (rnd() < 0.35f) { zx = (rnd() - 0.5f) * 0.3f; zy = (rnd() - 0.5f) * 0.2f; }
        else { zx = (rnd() * 2 - 1) * 0.8f; zy = (rnd() * 2 - 1) * 0.5f; }
        nextSacc = t + 0.5f + rnd() * 2.5f;
      }
    }
    if (autoOn && !closed && t >= nextBlink) {
      blink(0, 0.18f); blink(1, 0.18f);
      if (doppel) { doppel = false; nextBlink = t + 2.f + rnd() * 4.5f; }
      else if (rnd() < 0.15f) { doppel = true; nextBlink = t + 0.35f; }
      else nextBlink = t + 2.f + rnd() * 4.5f;
    }

    float kLook = 1 - expf(-22 * dt), kLid = 1 - expf(-10 * dt), kPup = 1 - expf(-4 * dt);
    for (int e = 0; e < 2; e++) {
      Lider z = ziel[e];
      if (closed) z.lidU = 1;
      EyeFrame& c = cur[e];
      float lx = zx + (e == 0 ? conv : -conv);
      c.lookX += (lx - c.lookX) * kLook;
      c.lookY += (zy - c.lookY) * kLook;
      c.lidU += (z.lidU - c.lidU) * kLid;
      c.lidL += (z.lidL - c.lidL) * kLid;
      c.curvL += (z.curvL - c.curvL) * kLid;
      c.tiltU += (z.tiltU - c.tiltU) * kLid;
      c.tiltL += (z.tiltL - c.tiltL) * kLid;
      c.pupil += (z.pupil - c.pupil) * kPup;
      if (blinkT[e] >= 0) { blinkT[e] += dt; if (blinkT[e] > blinkDur[e]) blinkT[e] = -1; }
    }
  }

  // Bild für Auge e (0 = links, 1 = rechts) inkl. Blinzeln
  EyeFrame frame(int e) const {
    EyeFrame f = cur[e];
    if (blinkT[e] >= 0) {
      float p = blinkT[e] / blinkDur[e];
      float c = p < 0.4f ? p / 0.4f : p < 0.55f ? 1.f : 1.f - (p - 0.55f) / 0.45f;
      c = c * c * (3 - 2 * c);
      f.lidU += (1 - f.lidU) * c;
    }
    return f;
  }

  // ---- Befehle ----
  void look(float x, float y) {
    zx = clampf(x, -1, 1); zy = clampf(y, -1, 1);
    manualUntil = t + 4.f;  // Automatik kurz pausieren
  }

  bool ausdruck(const char* name) {
    for (int i = 0; i < ANZ_AUSDRUECKE; i++) {
      if (strcmp(name, AUSDRUECKE[i].name) == 0) { setExpr(i); return true; }
    }
    return false;
  }

  bool aktion(const char* n) {
    if (!strcmp(n, "blinzeln")) { blink(0, 0.18f); blink(1, 0.18f); }
    else if (!strcmp(n, "zwinkern_l")) blink(0, 0.5f);
    else if (!strcmp(n, "zwinkern_r")) blink(1, 0.5f);
    else if (!strcmp(n, "zu")) closed = true;
    else if (!strcmp(n, "auf")) { closed = false; if (expr == ANZ_AUSDRUECKE - 1) setExpr(0); }
    else if (!strcmp(n, "rollen")) startSeq(1);
    else if (!strcmp(n, "umschauen")) startSeq(2);
    else if (!strcmp(n, "schielen")) startSeq(3);
    else if (!strcmp(n, "erschrecken")) startSeq(4);
    else if (!strcmp(n, "einschlafen")) setExpr(ANZ_AUSDRUECKE - 1);
    else if (!strcmp(n, "aufwachen")) { closed = false; setExpr(0); startSeq(2); blink(0, 0.3f); blink(1, 0.3f); }
    else return false;
    return true;
  }

  void setAuto(bool on) { autoOn = on; if (!on) { zx = zy = 0; } }
  bool autoMode() const { return autoOn; }
  bool zu() const { return closed; }
  const char* ausdruckName() const { return AUSDRUECKE[expr].name; }
  float blickX() const { return zx; }
  float blickY() const { return zy; }

 private:
  static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
  float rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (rs & 0xFFFFFF) / 16777216.f; }
  void blink(int e, float dur) { blinkT[e] = 0; blinkDur[e] = dur; }

  void setExpr(int i) {
    expr = i;
    ziel[0] = AUSDRUECKE[i].l; ziel[1] = AUSDRUECKE[i].r;
    if (i != ANZ_AUSDRUECKE - 1) closed = false;
  }

  void startSeq(int s) {
    if (seq == 4) setExpr(exprVorher);
    seq = s; seqT = 0; conv = 0;
    if (s == 4) {
      exprVorher = expr; setExpr(UEBERRASCHT);
      for (int e = 0; e < 2; e++) cur[e].pupil = ziel[e].pupil;
    }
  }

  void runSeq(float dt) {
    seqT += dt;
    float q = seqT;
    switch (seq) {
      case 1:  // Augen rollen
        if (q < 0.25f) { zx = 0; zy = -0.95f; }
        else if (q < 1.65f) { float a = (q - 0.25f) / 1.4f * 6.2832f; zx = sinf(a) * 0.95f; zy = -cosf(a) * 0.95f; }
        else if (q < 2.0f) { zx = 0; zy = 0; }
        else seq = 0;
        break;
      case 2:  // umschauen
        if (q < 0.8f) { zx = -0.95f; zy = 0.05f; }
        else if (q < 1.7f) { zx = 0.95f; zy = 0.05f; }
        else if (q < 2.2f) { zx = 0; zy = 0; }
        else seq = 0;
        break;
      case 3:  // schielen
        if (q < 1.8f) { conv = 0.8f; zx = 0; zy = 0.2f; }
        else { conv = 0; zy = 0; seq = 0; }
        break;
      case 4:  // erschrecken
        if (q > 1.4f) { setExpr(exprVorher); seq = 0; }
        break;
    }
    if (!seq) manualUntil = t + 0.8f;
  }

  EyeFrame cur[2];
  Lider ziel[2];
  float zx = 0, zy = 0, conv = 0;
  int expr = 0, exprVorher = 0;
  bool autoOn = true, closed = false, doppel = false;
  float t = 0, nextSacc = 1, nextBlink = 2.5f, manualUntil = 0;
  float blinkT[2] = {-1, -1}, blinkDur[2] = {0.18f, 0.18f};
  int seq = 0;
  float seqT = 0;
  uint32_t rs = 1;
};

}  // namespace augen
