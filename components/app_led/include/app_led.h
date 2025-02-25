#ifndef _APP_LED_H_
#define _APP_LED_H_

#include "led_strip.h"
#include "fsm.h"

//------------------------------------------------------//
//  MACRO definitions                                    //
//------------------------------------------------------//
/* Max number of leds in a strip */
#define MAX_STRIP_LEN 7

#define LED_TASK_PERIOD_MS 200
#define LED_TASK_PRIOR 2

#define LED_TIMER_PERIOD_MS 1

#define LED_BLINK_PERIOD (250 / LED_TIMER_PERIOD_MS)
//------------------------------------------------------//
//  TYPES DEFINITIONS                                    //
//------------------------------------------------------//

/**
 * @brief LED colour struct
 * 
 */
typedef struct
{
    struct 
    {
        uint32_t red;
        uint32_t green;
        uint32_t blue;
    } rgb;
    struct 
    {
        uint16_t hue;
        uint8_t saturation;
        uint8_t value;
    } hsv;
} led_colour_t;

/**
 * @brief LED instance struct
 * 
 */
typedef struct
{
    // led params
    led_strip_handle_t handle;
    led_strip_config_t strip_config;
    led_strip_spi_config_t spi_config;
    // fsm 
    fsm_t fsm;
    //timer
    TimerHandle_t timer;
    // 
    led_colour_t colour[MAX_STRIP_LEN]; 
}led_ins_t;

//------------------------------------------------------//
//  FUNCTIONS                                           //
//------------------------------------------------------//

void configure_led(led_ins_t *device);
void led_on(led_ins_t *device);
void led_off(led_ins_t *device);
void blink_led(led_ins_t *device);
void toggle_led(led_ins_t *device);
int  app_led_run(led_ins_t *device);
int app_led_update(led_ins_t *device, uint32_t index, led_colour_t *colour, uint32_t len);

#endif // _APP_LED_H_