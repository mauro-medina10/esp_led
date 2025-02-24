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
#define BTN_TASK_PRIOR 3

#define BTN_TASK_PERIOD_MS  10
#define BTN_ANTIBOUNCE_T    10  // 50 ms
#define BTN_LONG_PRESS_T    500 // 500 ms
#define BTN_MAX_EVENTS      10

// Timed events definitions
#define BTN_TIMER_PERIOD_MS 1

#define BTN_ANTI_BOUNCE_MS (BTN_ANTIBOUNCE_T / BTN_TIMER_PERIOD_MS)
#define BTN_LONG_PRESS_MS (BTN_LONG_PRESS_T / BTN_TIMER_PERIOD_MS)
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

/**
 * @brief MEF states
 * 
 */
typedef enum {
    ROOT_ST = FSM_ST_FIRST,
    INIT_ST,
    IDLE_ST,
    PRESS_ST,
    WAIT_ST,
    S_PRESS_ST,
    S_UNPRESS_ST,
    L_PRESS_ST,
} btn_st_t;

/**
 * @brief MEF events
 * 
 */
enum {
    READY_EV = FSM_EV_FIRST,
    PRESS_EV,
    UNPRESS_EV,
    TIMEOUT_EV,
    LAST_EV,
};

//------------------------------------------------------//
//  FUNCTIONS                                           //
//------------------------------------------------------//
int btn_configure(btn_ins_t *device, uint32_t gpio);
int btn_actor_link(btn_ins_t *device, struct fsm_actor_t* actor, int actor_len);
int btn_run(btn_ins_t *device);
btn_evt_t btn_wait_for_event(btn_ins_t *device, TickType_t maxWait);
#endif // _APP_BTN_H_