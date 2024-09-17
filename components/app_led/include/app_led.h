#ifndef _APP_LED_H_
#define _APP_LED_H_

#include "led_strip.h"

// DEFINITIONS
#define LED_MAX_STRIPS 10

/**
 * @brief LED colour union
 * 
 */
typedef union
{
    struct 
    {
        uint32_t red;
        uint32_t green;
        uint32_t blue;
    } rgb;
    struct 
    {
        uint32_t hue;
        uint32_t saturation;
        uint32_t value;
    } hsv;
} led_colour_t;

void configure_led(void);
void blink_led(void);
int  app_led_run(void);
void app_led_update(uint32_t index, led_colour_t colour);

#endif // _APP_LED_H_