#pragma once
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "soc/usb_serial_jtag_reg.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include <string>
// Stage 1 group scan for the Radar 360 panel. Roles: 0 CLK, 1 MOSI, 2 DC, 3 CS (held low), 4 RST (pulsed, then high), 5 = input (unused).
// TE is expected on GPIO15 (Stage 0: actively driven low). GPIO5 = backlight, GPIO38 = button: never touched here.
static const uint8_t G_INIT[] = {0xFE, 0x00, 0xEF, 0x00, 0x80, 0x01, 0x19, 0x82, 0x01, 0x09, 0x83, 0x01, 0x03, 0x88, 0x01, 0x00, 0x89, 0x01, 0x38, 0x8A, 0x01, 0x40, 0x8B, 0x01, 0x0A, 0x8C, 0x01, 0x00, 0x81, 0x01, 0xFF, 0x84, 0x01, 0xFF, 0x85, 0x01, 0xFF, 0x86, 0x01, 0xFF, 0x87, 0x01, 0xFF, 0x8E, 0x01, 0xFF, 0x8F, 0x01, 0xFF, 0x98, 0x01, 0x3E, 0x99, 0x01, 0x3E, 0x7D, 0x01, 0x72, 0x70, 0x0A, 0x02, 0x03, 0x03, 0x06, 0x03, 0x03, 0x09, 0x07, 0x09, 0x03, 0x90, 0x04, 0x06, 0x06, 0x01, 0x01, 0x93, 0x03, 0x02, 0xFF, 0x00, 0xCB, 0x01, 0x02, 0xFB, 0x02, 0x00, 0x00, 0xF6, 0x01, 0xC0, 0x6C, 0x07, 0x00, 0x00, 0x22, 0x00, 0xCC, 0x04, 0x58, 0xAA, 0x02, 0x0B, 0x00, 0xEC, 0x01, 0x07, 0xF9, 0x01, 0x40, 0xEB, 0x02, 0x01, 0x67, 0x74, 0x06, 0x01, 0x60, 0x00, 0x00, 0x00, 0x00, 0xB5, 0x03, 0x14, 0x14, 0x14, 0x6E, 0x20, 0x0B, 0x0B, 0x09, 0x09, 0x13, 0x13, 0x11, 0x11, 0x16, 0x15, 0x01, 0x04, 0x00, 0x0D, 0x1D, 0x00, 0x00, 0x1D, 0x0D, 0x00, 0x04, 0x08, 0x15, 0x16, 0x12, 0x12, 0x14, 0x14, 0x0A, 0x0A, 0x0C, 0x0C, 0x60, 0x04, 0x38, 0x1C, 0x13, 0x56, 0x61, 0x04, 0xF8, 0x0A, 0x13, 0x56, 0x62, 0x04, 0xF8, 0x0B, 0x13, 0x56, 0x63, 0x04, 0x38, 0x1C, 0x13, 0x56, 0x64, 0x06, 0x38, 0x20, 0x72, 0xF8, 0x13, 0x56, 0x65, 0x06, 0x78, 0x1A, 0x70, 0x0B, 0x56, 0x13, 0x66, 0x06, 0x38, 0x24, 0x72, 0xFC, 0x13, 0x56, 0x68, 0x07, 0xB3, 0x08, 0x0E, 0x08, 0x0E, 0x0A, 0x0A, 0x69, 0x07, 0xB3, 0x08, 0x0E, 0x08, 0x0E, 0x0A, 0x0A, 0x6A, 0x02, 0x00, 0x00, 0x3A, 0x01, 0x05, 0x36, 0x01, 0x00, 0x7C, 0x02, 0xB6, 0x29, 0xAC, 0x01, 0x40, 0xC3, 0x01, 0x1A, 0xC4, 0x01, 0x24, 0xC9, 0x01, 0x2F, 0xF0, 0x06, 0x11, 0x17, 0x08, 0x06, 0x05, 0x38, 0xF1, 0x06, 0x4D, 0x72, 0x72, 0x2D, 0x34, 0x8F, 0xF2, 0x06, 0x11, 0x17, 0x08, 0x06, 0x05, 0x38, 0xF3, 0x06, 0x4D, 0x72, 0x72, 0x2D, 0x34, 0x8F, 0xB4, 0x01, 0x0A, 0x35, 0x01, 0x00, 0xFE, 0x00, 0xEE, 0x00};
static const int G_POOL[] = {0,1,2,3,4,6,7,8,9,10,11,12,13,14,16,17,18,19,20,21,39,40,41,42,43,44,45,46,47,48};
static const int G_N = sizeof(G_POOL) / sizeof(G_POOL[0]);
static const int G_TE = 15;
static uint8_t g_role[G_N];
static uint32_t g_lo[6], g_hi[6];
static const char *G_NAMES[] = {"clk", "mosi", "dc", "cs", "rst"};

static uint32_t g_rng(uint32_t &s) { s += 0x9E3779B9u; uint32_t z = s; z = (z ^ (z >> 16)) * 0x85EBCA6Bu; z = (z ^ (z >> 13)) * 0xC2B2AE35u; return z ^ (z >> 16); }
static void g_colour(uint32_t attempt) { uint32_t s = attempt * 2654435761u + 0x5EED; for (int i = 0; i < G_N; i++) g_role[i] = g_rng(s) % 5; }
static void g_masks() {
  for (int r = 0; r < 6; r++) { g_lo[r] = 0; g_hi[r] = 0; }
  for (int i = 0; i < G_N; i++) { int p = G_POOL[i]; if (p < 32) g_lo[g_role[i]] |= 1u << p; else g_hi[g_role[i]] |= 1u << (p - 32); }
}
static inline void g_set(int r, int v) {
  if (v) { if (g_lo[r]) REG_WRITE(GPIO_OUT_W1TS_REG, g_lo[r]); if (g_hi[r]) REG_WRITE(GPIO_OUT1_W1TS_REG, g_hi[r]); }
  else { if (g_lo[r]) REG_WRITE(GPIO_OUT_W1TC_REG, g_lo[r]); if (g_hi[r]) REG_WRITE(GPIO_OUT1_W1TC_REG, g_hi[r]); }
}
static inline void g_nop() { __asm__ __volatile__("nop; nop; nop; nop; nop; nop; nop; nop;"); }
static void g_byte(uint8_t b) { for (int i = 7; i >= 0; i--) { g_set(1, (b >> i) & 1); g_nop(); g_set(0, 1); g_nop(); g_set(0, 0); } }
static void g_cmd(uint8_t c) { g_set(2, 0); g_byte(c); }
static void g_dat(uint8_t d) { g_set(2, 1); g_byte(d); }
static void g_pins_setup() {
  for (int i = 0; i < G_N; i++) {
    gpio_num_t p = (gpio_num_t) G_POOL[i];
    gpio_reset_pin(p);
    if (g_role[i] < 5) { gpio_set_direction(p, GPIO_MODE_OUTPUT); gpio_set_drive_capability(p, GPIO_DRIVE_CAP_0); }
    else { gpio_set_direction(p, GPIO_MODE_INPUT); gpio_set_pull_mode(p, GPIO_FLOATING); }
  }
}
static void g_release() { for (int i = 0; i < G_N; i++) { gpio_num_t p = (gpio_num_t) G_POOL[i]; gpio_reset_pin(p); gpio_set_direction(p, GPIO_MODE_INPUT); gpio_set_pull_mode(p, GPIO_FLOATING); } }
static void g_te_prep() { gpio_reset_pin((gpio_num_t) G_TE); gpio_set_direction((gpio_num_t) G_TE, GPIO_MODE_INPUT); gpio_set_pull_mode((gpio_num_t) G_TE, GPIO_FLOATING); }
static int g_te_edges(uint32_t us) {
  int c = 0; int prev = (REG_READ(GPIO_IN_REG) >> G_TE) & 1;
  int64_t end = esp_timer_get_time() + us;
  while (esp_timer_get_time() < end) { for (int r = 0; r < 200; r++) { int l = (REG_READ(GPIO_IN_REG) >> G_TE) & 1; if (l != prev) { c++; prev = l; } } }
  return c;
}
// one attempt with the current g_role; returns TE edges seen on GPIO15 in 60 ms
static uint8_t g_temode = 0;
static int g_attempt() {
  g_masks();
  g_pins_setup();
  g_set(0, 0); g_set(1, 0); g_set(2, 1); g_set(3, 1); g_set(4, 1);
  delay(2);
  g_set(4, 0); delay(10); g_set(4, 1); delay(120);
  g_set(3, 0);
  size_t i = 0;
  while (i < sizeof(G_INIT)) { uint8_t c = G_INIT[i++]; uint8_t n = G_INIT[i++]; g_cmd(c); for (uint8_t k = 0; k < n; k++) g_dat(G_INIT[i++]); }
  g_cmd(0x11); delay(120);
  g_cmd(0x35); g_dat(g_temode);
  g_cmd(0x29); delay(20);
  int e = g_te_edges(60000);
  esp_task_wdt_reset();
  return e;
}
static bool g_hit() { return g_attempt() >= 2; }
// after a confirmed hit: halve every role group down to one pin; role absent if the hit survives with the group empty
static std::string g_decode() {
  std::string out;
  for (int r = 0; r < 5; r++) {
    int idx[G_N]; int n = 0;
    for (int i = 0; i < G_N; i++) if (g_role[i] == r) idx[n++] = i;
    while (n > 1) {
      int h = n / 2;
      for (int k = h; k < n; k++) g_role[idx[k]] = 5;
      if (g_hit() && g_hit()) { n = h; continue; }
      for (int k = h; k < n; k++) g_role[idx[k]] = r;
      for (int k = 0; k < h; k++) g_role[idx[k]] = 5;
      if (g_hit() && g_hit()) { for (int k = 0; k < n - h; k++) idx[k] = idx[h + k]; n = n - h; continue; }
      for (int k = 0; k < h; k++) g_role[idx[k]] = r;
      break;  // neither half alone works - keep the group as is
    }
    char b[48];
    if (n == 1) {
      g_role[idx[0]] = 5;
      bool needed = !(g_hit() && g_hit());
      g_role[idx[0]] = r;
      snprintf(b, sizeof(b), "%s=%s%d ", G_NAMES[r], needed ? "" : "(not needed)", G_POOL[idx[0]]);
    } else {
      snprintf(b, sizeof(b), "%s=?%dpins ", G_NAMES[r], n);
    }
    out += b;
  }
  for (int i = 0; i < G_N; i++) if (g_role[i] == 5) {}  // unused pins stay inputs
  return out;
}
static std::string g_desc() {
  std::string s;
  for (int r = 0; r < 5; r++) { s += G_NAMES[r]; s += ":"; for (int i = 0; i < G_N; i++) if (g_role[i] == r) { s += std::to_string(G_POOL[i]); s += ","; } s += " "; }
  return s;
}
static void g_free_usb() { CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE | USB_SERIAL_JTAG_DP_PULLUP); }

// fill 360x360: top half 0xFF, bottom half 0x00 (visible whichever way colours are inverted)
static void g_fill() {
  g_cmd(0x2A); g_dat(0); g_dat(0); g_dat(0x01); g_dat(0x67);
  g_cmd(0x2B); g_dat(0); g_dat(0); g_dat(0x01); g_dat(0x67);
  g_cmd(0x2C);
  g_set(3, 0); g_set(2, 1);
  for (int row = 0; row < 360; row++) {
    uint8_t v = row < 180 ? 0xFF : 0x00;
    for (int k = 0; k < 720; k++) g_byte(v);
    if ((row & 31) == 0) esp_task_wdt_reset();
  }
}

// read-back: send cmd (DC low), then release the MOSI pin to input and clock nbits, sampling every GPIO on the
// clock-high phase. Returns per-pin bitstreams for MOSI (bidirectional SDA) and all role-5 (floating) pins.
static uint32_t g_rd_lo[48], g_rd_hi[48];
static void g_read_raw(uint8_t cmd, int nbits) {
  g_cmd(cmd);
  int mosi = -1; for (int i = 0; i < G_N; i++) if (g_role[i] == 1) mosi = G_POOL[i];
  gpio_set_direction((gpio_num_t) mosi, GPIO_MODE_INPUT);
  for (int b = 0; b < nbits; b++) {
    g_nop(); g_nop();
    g_set(0, 1); g_nop(); g_nop();
    g_rd_lo[b] = REG_READ(GPIO_IN_REG); g_rd_hi[b] = REG_READ(GPIO_IN1_REG);
    g_set(0, 0);
  }
  gpio_set_direction((gpio_num_t) mosi, GPIO_MODE_OUTPUT);
  g_set(3, 1); g_nop(); g_set(3, 0);  // end the read transaction
}
static uint32_t g_stream(int pin, int nbits) {
  uint32_t v = 0;
  for (int b = 0; b < nbits; b++) { int bit = pin < 32 ? (g_rd_lo[b] >> pin) & 1 : (g_rd_hi[b] >> (pin - 32)) & 1; v = (v << 1) | bit; }
  return v;
}
// find a pin whose 16-bit stream contains byte x at any offset 0..8
static int g_find(uint8_t x, int *off) {
  for (int i = 0; i < G_N; i++) {
    if (g_role[i] != 1 && g_role[i] != 5) continue;
    int p = G_POOL[i];
    uint32_t s = g_stream(p, 17);
    for (int o = 0; o <= 9; o++) if (((s >> (17 - 8 - o)) & 0xFF) == x) { *off = o; return p; }
  }
  return -1;
}

#include "xtensa/core-macros.h"
// precise TE timing over `us`: mean period (rising-to-rising) and mean high time, in microseconds x10
static void g_te_timing(uint32_t us, int *period_x10, int *high_x10, int *n_rise) {
  int prev = (REG_READ(GPIO_IN_REG) >> G_TE) & 1;
  int64_t end = esp_timer_get_time() + us;
  int64_t first_rise = -1, last_rise = -1, rise_t = -1; int rises = 0; int64_t high_sum = 0; int highs = 0;
  while (esp_timer_get_time() < end) {
    int l = (REG_READ(GPIO_IN_REG) >> G_TE) & 1;
    if (l != prev) {
      int64_t t = esp_timer_get_time();
      if (l) { if (first_rise < 0) first_rise = t; last_rise = t; rises++; rise_t = t; }
      else if (rise_t >= 0) { high_sum += t - rise_t; highs++; }
      prev = l;
    }
  }
  *n_rise = rises;
  *period_x10 = rises > 1 ? (int) ((last_rise - first_rise) * 10 / (rises - 1)) : -1;
  *high_x10 = highs ? (int) (high_sum * 10 / highs) : -1;
}
