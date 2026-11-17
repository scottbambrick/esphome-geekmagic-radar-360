#pragma once
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "soc/gpio_reg.h"
#include "soc/usb_serial_jtag_reg.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include <string>

static void st0_free_usb() {
  CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE | USB_SERIAL_JTAG_DP_PULLUP);
}
static int st0_lvl(int p) { return p < 32 ? ((REG_READ(GPIO_IN_REG) >> p) & 1) : ((REG_READ(GPIO_IN1_REG) >> (p - 32)) & 1); }

// ---- detector self-test: toggle a pin from an esp_timer at 120 Hz (60 Hz square), count edges by polling
static volatile int st0_tp = -1;
static volatile int st0_tv = 0;
static void st0_tcb(void *) { st0_tv ^= 1; gpio_set_level((gpio_num_t) st0_tp, st0_tv); }
static int st0_count(int p, uint32_t us) {
  int c = 0; int prev = st0_lvl(p);
  int64_t end = esp_timer_get_time() + us;
  while (esp_timer_get_time() < end) { for (int r = 0; r < 200; r++) { int l = st0_lvl(p); if (l != prev) { c++; prev = l; } } }
  return c;
}
static std::string st0_selftest(int p) {
  gpio_reset_pin((gpio_num_t) p);
  gpio_set_direction((gpio_num_t) p, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_drive_capability((gpio_num_t) p, GPIO_DRIVE_CAP_0);
  st0_tp = p;
  esp_timer_handle_t t; esp_timer_create_args_t a = {}; a.callback = st0_tcb; a.name = "st0";
  esp_timer_create(&a, &t); esp_timer_start_periodic(t, 8333);
  int n = st0_count(p, 60000);
  esp_timer_stop(t); esp_timer_delete(t);
  gpio_reset_pin((gpio_num_t) p); gpio_set_direction((gpio_num_t) p, GPIO_MODE_INPUT);
  char b[64]; snprintf(b, sizeof(b), "selftest gpio%d edges=%d (expect ~7)", p, n); return b;
}

// ---- pin characterisation
static adc_oneshot_unit_handle_t st0_u1 = nullptr, st0_u2 = nullptr;
static void st0_pull(int p, int m) { // 0 none, 1 up, 2 down
  gpio_set_pull_mode((gpio_num_t) p, m == 1 ? GPIO_PULLUP_ONLY : m == 2 ? GPIO_PULLDOWN_ONLY : GPIO_FLOATING);
  if (rtc_gpio_is_valid_gpio((gpio_num_t) p)) {
    if (m == 1) { rtc_gpio_pulldown_dis((gpio_num_t) p); rtc_gpio_pullup_en((gpio_num_t) p); }
    else if (m == 2) { rtc_gpio_pullup_dis((gpio_num_t) p); rtc_gpio_pulldown_en((gpio_num_t) p); }
    else { rtc_gpio_pullup_dis((gpio_num_t) p); rtc_gpio_pulldown_dis((gpio_num_t) p); }
  }
}
static int st0_mv(int p) {
  adc_unit_t unit; adc_channel_t ch;
  if (adc_oneshot_io_to_channel(p, &unit, &ch) != ESP_OK) return -1;
  adc_oneshot_unit_handle_t h = unit == ADC_UNIT_1 ? st0_u1 : st0_u2;
  if (!h) return -2;
  int best = -3;
  for (int tries = 0; tries < 20; tries++) { int raw; if (adc_oneshot_read(h, ch, &raw) == ESP_OK) { best = raw; break; } delay(2); }
  return best < 0 ? best : best * 3100 / 4095;
}
static std::string st0_pin(int p) {
  char b[48];
  adc_unit_t unit; adc_channel_t ch;
  bool analog = adc_oneshot_io_to_channel(p, &unit, &ch) == ESP_OK;
  if (analog) {
    if (!st0_u1) { adc_oneshot_unit_init_cfg_t c = {}; c.unit_id = ADC_UNIT_1; adc_oneshot_new_unit(&c, &st0_u1); }
    if (!st0_u2) { adc_oneshot_unit_init_cfg_t c = {}; c.unit_id = ADC_UNIT_2; adc_oneshot_new_unit(&c, &st0_u2); }
    adc_oneshot_chan_cfg_t cc = {}; cc.atten = ADC_ATTEN_DB_12; cc.bitwidth = ADC_BITWIDTH_12;
    adc_oneshot_config_channel(unit == ADC_UNIT_1 ? st0_u1 : st0_u2, ch, &cc);
    int v[3];
    for (int m = 0; m < 3; m++) { st0_pull(p, m == 0 ? 1 : m == 1 ? 2 : 0); delay(5); v[m] = st0_mv(p); }
    st0_pull(p, 0);
    snprintf(b, sizeof(b), "%d:u%d d%d f%d", p, v[0], v[1], v[2]);
  } else {
    gpio_reset_pin((gpio_num_t) p); gpio_set_direction((gpio_num_t) p, GPIO_MODE_INPUT);
    st0_pull(p, 1); delay(5); int u = st0_lvl(p);
    st0_pull(p, 2); delay(5); int d = st0_lvl(p);
    st0_pull(p, 0); delay(5); int f = st0_lvl(p);
    snprintf(b, sizeof(b), "%d:U%d D%d F%d", p, u, d, f);
  }
  return b;
}
