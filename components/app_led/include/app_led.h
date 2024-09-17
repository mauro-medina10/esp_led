#ifndef _APP_LED_H_
#define _APP_LED_H_

#include "led_strip.h"

// DEFINITIONS
#define LED_MAX_STRIPS 10

void configure_led(void);
uint8_t blink_led(void);
void app_led_run(void);

#endif // _APP_LED_H_