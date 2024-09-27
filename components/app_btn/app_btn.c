
#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"

#include "app_btn.h"
#include "fsm.h"

static const char *TAG = "app_button";

//------------------------------------------------------//
//  FSM declarations                                    //
//------------------------------------------------------//

/**
 * @brief MEF states
 * 
 */
enum {
    ROOT_ST = FSM_ST_FIRST,
    INIT_ST,
    IDLE_ST,
    PRESS_ST,
    WAIT_ST,
    S_PRESS_ST,
    L_PRESS_ST,
};

/**
 * @brief MEF events
 * 
 */
enum {
    READY_EV = FSM_EV_FIRST,
    PRESS_EV,
    UNPRESS_EV,
    TIMEOUT_EV,
};

// Action function prototypes
static void enter_init      (fsm_t *self, void* data);
static void enter_wait      (fsm_t *self, void* data);
static void enter_press     (fsm_t *self, void* data);
static void enter_long_press(fsm_t *self, void* data);
static void exit_press      (fsm_t *self, void* data);

// Define FSM states
FSM_STATES_INIT(btn_fsm)
//                  name  state id          parent           sub         entry            run     exit
FSM_CREATE_STATE(btn_fsm, ROOT_ST,          FSM_ST_NONE,   INIT_ST,      NULL,            NULL,   NULL)
FSM_CREATE_STATE(btn_fsm, INIT_ST,          ROOT_ST,       FSM_ST_NONE, enter_init,       NULL,   NULL)
FSM_CREATE_STATE(btn_fsm, IDLE_ST,          ROOT_ST,       FSM_ST_NONE,  NULL,            NULL,   NULL)
FSM_CREATE_STATE(btn_fsm, PRESS_ST,         ROOT_ST,       WAIT_ST,      NULL,            NULL,   exit_press)
FSM_CREATE_STATE(btn_fsm, WAIT_ST,          PRESS_ST,      FSM_ST_NONE, enter_wait,       NULL,   NULL)
FSM_CREATE_STATE(btn_fsm, S_PRESS_ST,       PRESS_ST,      FSM_ST_NONE, enter_press,      NULL,   NULL)
FSM_CREATE_STATE(btn_fsm, L_PRESS_ST,       PRESS_ST,       FSM_ST_NONE,enter_long_press, NULL,   NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(btn_fsm)
//                    fsm name State source   event       state target
FSM_TRANSITION_CREATE(btn_fsm,   INIT_ST,     READY_EV,    IDLE_ST)
FSM_TRANSITION_CREATE(btn_fsm,   IDLE_ST,     PRESS_EV,    PRESS_ST)
FSM_TRANSITION_CREATE(btn_fsm,   PRESS_ST,    UNPRESS_EV,  IDLE_ST)
FSM_TRANSITION_CREATE(btn_fsm,   L_PRESS_ST,  UNPRESS_EV,  IDLE_ST)
FSM_TRANSITION_CREATE(btn_fsm,   WAIT_ST,     TIMEOUT_EV,  S_PRESS_ST)
FSM_TRANSITION_CREATE(btn_fsm,   S_PRESS_ST,  TIMEOUT_EV,  L_PRESS_ST)
FSM_TRANSITIONS_END()

//------------------------------------------------------//
//  APP declarations                                    //
//------------------------------------------------------//


//------------------------------------------------------//
//  LOCAL functions                                     //
//------------------------------------------------------//
/**
 * @brief Interrupt handler
 * 
 * @param arg 
 */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    btn_ins_t * btn = (btn_ins_t *) arg;
    if(btn == NULL)
    {
        ESP_LOGE(TAG, "ISR error");
        return;
    }

    if(gpio_get_level(btn->gpio) == 0)
    {
        fsm_dispatch(&btn->fsm, PRESS_EV, btn);
    }else
    {
        fsm_dispatch(&btn->fsm, UNPRESS_EV, btn);
    }
}

/**
 * @brief Internal task for running fsm every 10ms
 * 
 * @param arg 
 */
static void internal_task(void* arg)
{
    btn_ins_t * btn = (btn_ins_t *) arg;
    if(btn == NULL) return;

    for(;;)
    {
        btn_run(btn);

        vTaskDelay(BTN_TASK_PERIOD_MS / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Internal timer callback
 * 
 * @param xTimer 
 */
static void TimerCallback(TimerHandle_t xTimer)
{
    btn_ins_t * btn = (btn_ins_t *) pvTimerGetTimerID(xTimer);

    if(++btn->internal_count > btn->max_count)
    {
        ESP_LOGI(TAG, "btn timer event %d", (int)btn->max_count);

        btn->internal_count = 0;

        xTimerStop(xTimer, 0);

        fsm_dispatch(&btn->fsm, TIMEOUT_EV, btn);
    }
}

//------------------------------------------------------//
//  FSM functions                                       //
//------------------------------------------------------//
/**
 * @brief Initialice the button internal variables
 * 
 * @param self 
 * @param data 
 */
static void enter_init(fsm_t *self, void* data)
{
    btn_ins_t * btn = (btn_ins_t *) data;
    BaseType_t result;

    // GPIO init
    btn->io_conf.intr_type = GPIO_INTR_ANYEDGE;
    btn->io_conf.pin_bit_mask = (1ULL << btn->gpio);
    btn->io_conf.mode = GPIO_MODE_INPUT;
    btn->io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&btn->io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(btn->gpio, gpio_isr_handler, data);
    
    // Queu init
    btn->evt_q = xQueueCreate(BTN_MAX_EVENTS, sizeof(btn_evt_t));
    if(btn->evt_q == NULL)
    {
        ESP_LOGE(TAG, "Queu error");
    
    }
    // Timer init
    btn->internal_count = 0;
    btn->timer = xTimerCreate("btn_timer", pdMS_TO_TICKS(10), pdTRUE, data, TimerCallback);
    xTimerStop(btn->timer, 0);
    if(btn->timer == NULL)
    {
        ESP_LOGE(TAG, "Timer error");
    }
    // Task init
    // result = xTaskCreate(internal_task, "btn_task", 1024, data, tskIDLE_PRIORITY+2, NULL);
    // if(result != pdPASS)
    // {
    //     ESP_LOGE(TAG, "Task error");
    //     return;
    // }

    fsm_dispatch(&btn->fsm, READY_EV, btn);
}

/**
 * @brief Starts antibounce timer
 * 
 * @param self 
 * @param data 
 */
static void enter_wait(fsm_t *self, void* data)
{
    btn_ins_t * btn = (btn_ins_t *) data;

    btn->evt = BOUNCE_EV;
    // Sets timer target to antibounce time
    btn->max_count = BTN_ANTIBOUNCE_T;

    xTimerStart(btn->timer, 0);
}

/**
 * @brief Starts the long press timer
 * 
 * @param self 
 * @param data 
 */
static void enter_press(fsm_t *self, void* data)
{
    btn_ins_t * btn = (btn_ins_t *) data;
    
    // Sets event as pressed
    btn->evt = PRESSED_EV;

    // Sets timer target to long press time
    btn->max_count = BTN_LONG_PRESS_T;

    xTimerStart(btn->timer, 0);
}

/**
 * @brief Sends the long press event
 * 
 * @param self 
 * @param data 
 */
static void enter_long_press(fsm_t *self, void* data)
{
    btn_ins_t * btn = (btn_ins_t *) data;

    btn->evt = LONG_PRESS_EV;

    xTimerStop(btn->timer, 0);

    // Sends long press event
    xQueueSend(btn->evt_q, &btn->evt, 0);
}

/**
 * @brief Stops the timer 
 * 
 * @param self 
 * @param data 
 */
static void exit_press(fsm_t *self, void* data)
{
    btn_ins_t * btn = (btn_ins_t *) data;

    btn->internal_count = 0;

    xTimerStop(btn->timer, 0);

    // Sends long press event
    if(btn->evt != LONG_PRESS_EV) xQueueSend(btn->evt_q, &btn->evt, 0);
}

//------------------------------------------------------//
//  APP functions                                       //
//------------------------------------------------------//

/**
 * @brief Configures button instance
 * 
 * @param btn 
 * @return int 
 */
int btn_configure(btn_ins_t *device, uint32_t gpio)
{
    if(device == NULL) return -1;
    
    device->gpio = gpio;

    fsm_init(&device->fsm, FSM_TRANSITIONS_GET(btn_fsm), FSM_TRANSITIONS_SIZE(btn_fsm),
             &FSM_STATE_GET(btn_fsm, ROOT_ST), device);

    return 0;
}

/**
 * @brief Runs button FSM
 * 
 * @param device 
 * @return int 
 */
int btn_run(btn_ins_t *device)
{
    if(device == NULL) return -1;

    return fsm_run(&device->fsm); 
}

/**
 * @brief Waits for a button pressing event
 * 
 * @param device 
 * @param maxWait 
 * @return btn_evt_t 
 */
btn_evt_t btn_wait_for_event(btn_ins_t *device, TickType_t maxWait)
{
    btn_evt_t evt;
    
    if(device->evt_q == NULL) return NO_EV;

    xQueueReceive(device->evt_q, &evt, maxWait);

    return evt;
} 