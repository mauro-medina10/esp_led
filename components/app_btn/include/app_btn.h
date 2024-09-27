#ifndef _APP_BTN_H_
#define _APP_BTN_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "fsm.h"

//------------------------------------------------------//
//  MACRO definitions                                    //
//------------------------------------------------------//
#define BTN_TASK_PERIOD_MS  10
#define BTN_ANTIBOUNCE_T    5  // 50 ms
#define BTN_LONG_PRESS_T    50 // 500 ms
#define BTN_MAX_EVENTS      10
//------------------------------------------------------//
//  TYPES DEFINITIONS                                    //
//------------------------------------------------------//

/**
 * @brief Button event types
 * 
 */
typedef enum
{
    NO_EV = 0,
    BOUNCE_EV,
    PRESSED_EV,
    LONG_PRESS_EV,
}btn_evt_t;

/**
 * @brief Button instance
 * 
 */
typedef struct
{
    // fsm 
    fsm_t fsm;
    // queu
    QueueHandle_t evt_q;
    // Timer
    TimerHandle_t timer;
    uint32_t internal_count;
    uint32_t max_count;
    // Button gpio
    gpio_config_t io_conf;
    uint32_t gpio;
    // Last Event
    btn_evt_t evt;
}btn_ins_t;

//------------------------------------------------------//
//  FUNCTIONS                                           //
//------------------------------------------------------//
int btn_configure(btn_ins_t *device, uint32_t gpio);
int btn_run(btn_ins_t *device);
btn_evt_t btn_wait_for_event(btn_ins_t *device, TickType_t maxWait);
#endif // _APP_BTN_H_