#pragma once
// Zwei GC9A01-Displays (240x240, rund) an einem SPI-Bus: SCK, MOSI, DC und RST
// gemeinsam, je ein eigener CS. Pixeldaten gehen per DMA raus, während der
// Renderer schon den nächsten Streifen rechnet.
#include <Arduino.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"

namespace augen {

class Gc9a01Bus {
 public:
  bool begin(int sck, int mosi, int dc, int rst, int maxBytes) {
    s_dc = dc;
    pinMode(dc, OUTPUT);
    if (rst >= 0) {  // gemeinsamer Reset für beide Displays
      pinMode(rst, OUTPUT);
      digitalWrite(rst, HIGH); delay(5);
      digitalWrite(rst, LOW); delay(20);
      digitalWrite(rst, HIGH); delay(150);
    }
    spi_bus_config_t bus = {};
    bus.mosi_io_num = mosi;
    bus.miso_io_num = -1;
    bus.sclk_io_num = sck;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = maxBytes + 8;
    return spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) == ESP_OK;
  }

  // Display am Bus anmelden und initialisieren. madctl: Drehung/Farbreihenfolge.
  bool addPanel(int cs, uint32_t hz, uint8_t madctl) {
    if (n >= 2) return false;
    spi_device_interface_config_t dev = {};
    dev.clock_speed_hz = (int)hz;
    dev.mode = 0;
    dev.spics_io_num = cs;
    dev.queue_size = 7;
    dev.pre_cb = preCb;
    if (spi_bus_add_device(SPI2_HOST, &dev, &devs[n]) != ESP_OK) return false;
    initPanel(devs[n], madctl);
    n++;
    return true;
  }

  // Streifen (volle Breite) asynchron senden. Wartet vorher auf den vorigen Streifen,
  // deshalb abwechselnd zwei Puffer benutzen.
  void sendStrip(int panel, int y0, int rows, const uint16_t* data) {
    finish();
    spi_device_handle_t d = devs[panel];
    int y1 = y0 + rows - 1;
    setCmd(0, 0x2A); setData4(1, 0, 0, 0, 239);
    setCmd(2, 0x2B); setData4(3, y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF);
    setCmd(4, 0x2C);
    spi_transaction_t& px = tr[5];
    px = {};
    px.length = rows * 240 * 16;
    px.tx_buffer = data;
    px.user = (void*)1;
    for (int i = 0; i < 6; i++) spi_device_queue_trans(d, &tr[i], portMAX_DELAY);
    pendDev = d;
    pend = 6;
  }

  void finish() {
    spi_transaction_t* r;
    while (pend > 0) { spi_device_get_trans_result(pendDev, &r, portMAX_DELAY); pend--; }
  }

  // Beide Displays schlafen legen (Bild schwarz) oder wecken. Die Hintergrundbeleuchtung
  // hängt bei diesen Modulen fest an VCC und bleibt an.
  void power(bool on) {
    finish();
    for (int i = 0; i < n; i++) {
      uint8_t c = on ? 0x11 : 0x28;  // Sleep out / Display aus
      write(devs[i], false, &c, 1);
    }
    delay(on ? 120 : 5);
    for (int i = 0; i < n; i++) {
      uint8_t c = on ? 0x29 : 0x10;  // Display an / Sleep in
      write(devs[i], false, &c, 1);
    }
  }

  static uint16_t* allocBuffer(size_t bytes) {
    return (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_DMA);
  }

 private:
  static void IRAM_ATTR preCb(spi_transaction_t* t) { gpio_set_level((gpio_num_t)s_dc, (int)(intptr_t)t->user); }

  void setCmd(int i, uint8_t c) {
    tr[i] = {};
    tr[i].length = 8;
    tr[i].flags = SPI_TRANS_USE_TXDATA;
    tr[i].tx_data[0] = c;
    tr[i].user = (void*)0;
  }
  void setData4(int i, uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    tr[i] = {};
    tr[i].length = 32;
    tr[i].flags = SPI_TRANS_USE_TXDATA;
    tr[i].tx_data[0] = a; tr[i].tx_data[1] = b; tr[i].tx_data[2] = c; tr[i].tx_data[3] = d;
    tr[i].user = (void*)1;
  }

  void write(spi_device_handle_t d, bool isData, const uint8_t* buf, int len) {
    if (len == 0) return;
    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = buf;
    t.user = (void*)(intptr_t)(isData ? 1 : 0);
    spi_device_polling_transmit(d, &t);
  }

  void initPanel(spi_device_handle_t d, uint8_t madctl) {
    // Init-Sequenz nach Waveshare/GC9A01-Datenblatt: {Befehl, Anzahl Daten, Daten...}
    static const uint8_t seq[] = {
      0xEF, 0, 0xEB, 1, 0x14, 0xFE, 0, 0xEF, 0, 0xEB, 1, 0x14,
      0x84, 1, 0x40, 0x85, 1, 0xFF, 0x86, 1, 0xFF, 0x87, 1, 0xFF, 0x88, 1, 0x0A,
      0x89, 1, 0x21, 0x8A, 1, 0x00, 0x8B, 1, 0x80, 0x8C, 1, 0x01, 0x8D, 1, 0x01,
      0x8E, 1, 0xFF, 0x8F, 1, 0xFF, 0xB6, 2, 0x00, 0x20,
      0x3A, 1, 0x05, 0x90, 4, 0x08, 0x08, 0x08, 0x08, 0xBD, 1, 0x06, 0xBC, 1, 0x00,
      0xFF, 3, 0x60, 0x01, 0x04, 0xC3, 1, 0x13, 0xC4, 1, 0x13, 0xC9, 1, 0x22,
      0xBE, 1, 0x11, 0xE1, 2, 0x10, 0x0E, 0xDF, 3, 0x21, 0x0C, 0x02,
      0xF0, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A, 0xF1, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
      0xF2, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A, 0xF3, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
      0xED, 2, 0x1B, 0x0B, 0xAE, 1, 0x77, 0xCD, 1, 0x63,
      0x70, 9, 0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03, 0xE8, 1, 0x34,
      0x62, 12, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70,
      0x63, 12, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70,
      0x64, 7, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07,
      0x66, 10, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00,
      0x67, 10, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98,
      0x74, 7, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00, 0x98, 2, 0x3E, 0x07,
      0x35, 0, 0x21, 0,
    };
    for (size_t i = 0; i < sizeof(seq);) {
      uint8_t c = seq[i], len = seq[i + 1];
      write(d, false, &c, 1);
      write(d, true, &seq[i + 2], len);
      i += 2 + len;
    }
    uint8_t m = madctl;
    uint8_t cmd = 0x36; write(d, false, &cmd, 1); write(d, true, &m, 1);
    cmd = 0x11; write(d, false, &cmd, 1); delay(120);  // Sleep out
    cmd = 0x29; write(d, false, &cmd, 1); delay(20);   // Display an
  }

  static inline int s_dc = -1;
  spi_device_handle_t devs[2] = {};
  int n = 0;
  spi_transaction_t tr[6];
  spi_device_handle_t pendDev = nullptr;
  int pend = 0;
};

}  // namespace augen
