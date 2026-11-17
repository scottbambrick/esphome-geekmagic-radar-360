#pragma once
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
static const int BTN_PINS[] = {1,2,3,4,6,7,8,9,10,11,12,13,14,15,16,17,18,21,38,39,40,41,42,45,46,47,48};
static uint8_t btn_state[49];
static uint8_t btn_pend[49];
static uint8_t btn_init = 0;
static void btn_setup() { for (size_t k = 0; k < sizeof(BTN_PINS) / sizeof(BTN_PINS[0]); k++) { int p = BTN_PINS[k]; gpio_reset_pin((gpio_num_t) p); gpio_set_direction((gpio_num_t) p, GPIO_MODE_INPUT); if (p != 15) gpio_set_pull_mode((gpio_num_t) p, GPIO_PULLUP_ONLY); } }
static int btn_level(int p) { return p < 32 ? ((REG_READ(GPIO_IN_REG) >> p) & 1) : ((REG_READ(GPIO_IN1_REG) >> (p - 32)) & 1); }
