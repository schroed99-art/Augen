#pragma once
// Augen-Renderer: rechnet ein Auge zeilenweise in RGB565 (byte-getauscht fürs SPI).
// Reines C++ ohne Arduino-Abhängigkeit – lässt sich am Rechner testen (tools/host_test.cpp).
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

namespace augen {

constexpr int W = 240, H = 240, MID = 120;
constexpr int IRIS_R = 62, IRIS_D = IRIS_R * 2;
constexpr float BLICK_WEG = 40.0f;  // max. Auslenkung der Iris in Pixeln

enum Stil : uint8_t { MENSCH = 0, KATZE = 1, ROBOTER = 2 };

// Zustand eines Auges für genau ein Bild
struct EyeFrame {
  float lookX = 0, lookY = 0;  // Blick -1..1 (x: rechts, y: unten)
  float pupil = 0.5f;          // 0 eng .. 1 weit
  float lidU = 0, lidL = 0;    // Schließgrad Ober-/Unterlid 0..1
  float curvL = 0;             // Unterlid: 0 natürlich, 1 "lächelnd" (Mitte hoch)
  float tiltU = 0, tiltL = 0;  // >0: innerer Augenwinkel tiefer
};

static inline int clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static inline uint16_t rgb565(int r, int g, int b) {
  r = clamp8(r); g = clamp8(g); b = clamp8(b);
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
// f: 0..256
static inline uint16_t scale565(uint16_t c, int f) {
  int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  return (uint16_t)((((r * f) >> 8) << 11) | (((g * f) >> 8) << 5) | ((b * f) >> 8));
}
static inline uint16_t swap16(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

class Renderer {
 public:
  bool begin() {
    iris = (uint16_t*)malloc(IRIS_D * IRIS_D * sizeof(uint16_t));
    return iris != nullptr;
  }

  void setStyle(Stil s, uint32_t irisRGB, uint32_t lidRGB) {
    st = s;
    int ir = (irisRGB >> 16) & 255, ig = (irisRGB >> 8) & 255, ib = irisRGB & 255;
    int lr = (lidRGB >> 16) & 255, lg = (lidRGB >> 8) & 255, lb = lidRGB & 255;
    lidCol = rgb565(lr, lg, lb);
    lashCol = rgb565(lr * 2 / 5, lg * 2 / 5, lb * 2 / 5);
    pupCol = rgb565(8, 8, 12);
    irisCol = rgb565(ir, ig, ib);
    irisDim = rgb565(ir * 2 / 5, ig * 2 / 5, ib * 2 / 5);
    irisDark = rgb565(ir / 8, ig / 8, ib / 8);
    katzRand = rgb565(ir / 3, ig / 4, ib / 6);

    for (int k = 0; k < 128; k++) {  // Augenweiß, zum Rand hin dunkler
      float r = sqrtf(k * 256.f);
      float f = r < 78 ? 1.f : 1.f - 0.5f * powf(fminf((r - 78) / 44.f, 1.f), 1.4f);
      sclera[k] = rgb565((int)(248 * f + (1 - f) * 70), (int)(242 * f), (int)(236 * f));
    }
    for (int k = 0; k < 256; k++) {  // Katzeniris, radialer Verlauf
      float d = sqrtf(k * 256.f);
      float t = 1.1f - 0.55f * fminf(d / 150.f, 1.f);
      if (d < 40) t += 0.15f * (1 - d / 40);
      katze[k] = rgb565((int)(ir * t), (int)(ig * t), (int)(ib * t));
    }
    if (!iris) return;
    for (int j = 0; j < IRIS_D; j++) {  // Menschliche Iris mit Fasern, Krause und Limbusring
      for (int i = 0; i < IRIS_D; i++) {
        float dx = i - IRIS_R + 0.5f, dy = j - IRIS_R + 0.5f;
        float d = sqrtf(dx * dx + dy * dy) / IRIS_R;
        if (d > 1) { iris[j * IRIS_D + i] = 0; continue; }
        float a = atan2f(dy, dx);
        float f1 = 0.5f + 0.5f * sinf(a * 29 + 1.8f * sinf(a * 5 + d * 4));
        float f2 = 0.5f + 0.5f * sinf(a * 47 + 2.5f * sinf(a * 3) - d * 7);
        float t = 0.45f + 0.35f * f1 * f2 + 0.2f * (d - 0.4f);
        float k = (d - 0.42f) / 0.06f;
        t += 0.18f * expf(-k * k);
        if (d < 0.3f) t *= 0.85f;
        if (d > 0.84f) t *= 1 - 0.75f * (d - 0.84f) / 0.16f;
        float w = t > 0.7f ? (t - 0.7f) * 1.5f : 0;
        float r = fminf(ir * t * 1.2f, 255), g = fminf(ig * t * 1.2f, 255), b = fminf(ib * t * 1.2f, 255);
        iris[j * IRIS_D + i] = rgb565((int)(r + (255 - r) * w), (int)(g + (255 - g) * w), (int)(b + (255 - b) * w));
      }
    }
  }

  Stil stil() const { return st; }

  // Lidkanten und Pupille für ein Auge vorberechnen. rechts: inneres Eck liegt links.
  void beginFrame(const EyeFrame& f, bool rechts) {
    float s = rechts ? -1.f : 1.f;
    float ly = f.lookY * 14.f;  // Lider folgen dem Blick ein wenig
    float lidU = fminf(fmaxf(f.lidU, 0.f), 1.f);
    float baseL = 250.f + (118.f - 250.f) * f.lidL + ly * 0.5f;
    float curvL = f.curvL * 60.f - 35.f;
    for (int x = 0; x < W; x++) {
      float u = (x - 119.5f) / 120.f, uu = u * u;
      float yl = baseL + curvL * uu + f.tiltL * 45.f * u * s;
      float yo = -25.f + ly + 45.f * uu + f.tiltU * 70.f * u * s;
      edgeU[x] = (int16_t)(yo + (yl - yo) * lidU);
      edgeL[x] = (int16_t)yl;
    }
    cx = MID + (int)lroundf(f.lookX * BLICK_WEG);
    cy = MID + (int)lroundf(f.lookY * BLICK_WEG);
    float p = fminf(fmaxf(f.pupil, 0.f), 1.f);
    if (st == KATZE) {
      int sw = 5 + (int)(38 * p), sh = 92;
      sw2 = sw * sw; sh2 = sh * sh;
      rw2 = (sw + 5) * (sw + 5); rh2 = (sh + 6) * (sh + 6);
    } else {
      int pr = st == MENSCH ? 15 + (int)(22 * p) : 12 + (int)(22 * p);
      pr2 = pr * pr;
    }
  }

  // rows Zeilen ab y0 in out (W*rows Pixel, byte-getauscht) schreiben
  void renderRows(uint16_t* out, int y0, int rows) const {
    for (int y = y0; y < y0 + rows; y++) {
      int dyc = y - MID;
      for (int x = 0; x < W; x++) {
        int dxc = x - MID, r2 = dxc * dxc + dyc * dyc;
        if (r2 > 121 * 121) { *out++ = 0; continue; }  // außerhalb des runden Displays
        int eu = edgeU[x], el = edgeL[x];
        uint16_t c;
        if (y < eu) c = y >= eu - 3 ? lashCol : lidCol;
        else if (y >= el) c = y < el + 3 ? lashCol : lidCol;
        else {
          c = ball(x, y, r2);
          int du = y - eu, dl = el - y;
          if (du < 16) c = scale565(c, 110 + du * 9);  // Schatten unter dem Oberlid
          else if (dl < 5) c = scale565(c, 190 + dl * 13);
        }
        *out++ = swap16(c);
      }
    }
  }

 private:
  uint16_t ball(int x, int y, int r2) const {
    int dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
    switch (st) {
      case MENSCH: {
        if (d2 < IRIS_R * IRIS_R) {
          int hx = dx + 21, hy = dy + 23;
          if (hx * hx + hy * hy < 90) return 0xFFFF;  // Glanzlicht
          int gx = dx - 16, gy = dy - 14;
          if (gx * gx + gy * gy < 12) return 0xE73C;
          if (d2 < pr2) return pupCol;
          return iris[(dy + IRIS_R) * IRIS_D + dx + IRIS_R];
        }
        int k = r2 >> 8;
        return sclera[k > 127 ? 127 : k];
      }
      case KATZE: {
        int hx = dx + 30, hy = dy + 40;
        if (hx * hx + hy * hy < 120) return 0xFFFF;
        int xx = dx * dx, yy = dy * dy;
        if (xx * sh2 + yy * sw2 < sw2 * sh2) return pupCol;      // Schlitzpupille
        if (xx * rh2 + yy * rw2 < rw2 * rh2) return katzRand;    // dunkler Saum
        int k = d2 >> 8;
        return katze[k > 255 ? 255 : k];
      }
      default: {  // ROBOTER
        uint16_t bg = (y & 3) == 0 ? 0x0000 : 0x0841;  // Scanlines
        if (d2 < 16) return 0xFFFF;
        if (d2 < pr2) return irisCol;
        if (d2 < 50 * 50) return irisDark;
        if (d2 < 56 * 56) return bg;
        if (d2 < 68 * 68) return d2 < 60 * 60 ? irisCol : irisDim;
        return bg;
      }
    }
  }

  Stil st = MENSCH;
  uint16_t* iris = nullptr;
  uint16_t sclera[128];
  uint16_t katze[256];
  uint16_t lidCol = 0, lashCol = 0, pupCol = 0, irisCol = 0, irisDim = 0, irisDark = 0, katzRand = 0;
  int16_t edgeU[W], edgeL[W];
  int cx = MID, cy = MID, pr2 = 0, sw2 = 1, sh2 = 1, rw2 = 1, rh2 = 1;
};

}  // namespace augen
