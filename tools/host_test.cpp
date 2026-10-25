// Rendert Testbilder der Augen als BMP (am Rechner, ohne ESP32):
//   c++ -std=c++17 -O2 -I Augen tools/host_test.cpp -o /tmp/augen_test && /tmp/augen_test ausgabe/
#include <cstdio>
#include <string>
#include <vector>
#include "augen_render.h"
#include "augen_anim.h"
using namespace augen;

static void writeBmp(const std::string& fn, const std::vector<uint16_t>& px, int w, int h) {
  FILE* f = fopen(fn.c_str(), "wb");
  int row = w * 3, size = 54 + row * h;
  uint8_t hd[54] = {'B', 'M'};
  *(uint32_t*)(hd + 2) = size; *(uint32_t*)(hd + 10) = 54; *(uint32_t*)(hd + 14) = 40;
  *(int32_t*)(hd + 18) = w; *(int32_t*)(hd + 22) = -h; *(uint16_t*)(hd + 26) = 1; *(uint16_t*)(hd + 28) = 24;
  fwrite(hd, 1, 54, f);
  for (int i = 0; i < w * h; i++) {
    uint16_t c = swap16(px[i]);
    uint8_t b[3] = {(uint8_t)((c & 31) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c >> 11) << 3)};
    fwrite(b, 1, 3, f);
  }
  fclose(f);
}

int main(int argc, char** argv) {
  std::string out = argc > 1 ? argv[1] : "./";
  Renderer r; r.begin();
  struct S { Stil s; uint32_t iris, lid; const char* n; } stile[] = {
    {MENSCH, 0x3C82C8, 0x1E1614, "mensch"}, {KATZE, 0xFFC414, 0x0A0A0E, "katze"}, {ROBOTER, 0x00DCFF, 0x14181E, "roboter"}};
  const int GAP = 20, TW = W * 2 + GAP;
  for (auto& st : stile) {
    r.setStyle(st.s, st.iris, st.lid);
    std::vector<uint16_t> sheet((size_t)TW * H * ANZ_AUSDRUECKE, 0);
    for (int a = 0; a < ANZ_AUSDRUECKE; a++) {
      Engine e; e.begin(7); e.setAuto(false);
      e.ausdruck(AUSDRUECKE[a].name);
      e.look(0.35f, -0.15f);
      for (int i = 0; i < 120; i++) e.update(0.033f);
      for (int eye = 0; eye < 2; eye++) {
        std::vector<uint16_t> buf(W * H);
        r.beginFrame(e.frame(eye), eye == 1);
        r.renderRows(buf.data(), 0, H);
        for (int y = 0; y < H; y++)
          for (int x = 0; x < W; x++)
            sheet[(size_t)(a * H + y) * TW + eye * (W + GAP) + x] = buf[y * W + x];
      }
    }
    writeBmp(out + st.n + ".bmp", sheet, TW, H * ANZ_AUSDRUECKE);
  }
  puts("ok");
}
