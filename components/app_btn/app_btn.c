
#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"

#include "app_btn.h"
#include "fsm.h"

static const char *TAG = "app_button";

static void timed_events_cb(TimerHandle_t xTimer);

//------------------------------------------------------//
//  FSM declarations                                    //
//------------------------------------------------------//

// Action function prototypes
static void enter_init      (fsm_t *self, void* data);
static void enter_idle      (fsm_t *self, void* data);
static void enter_wait      (fsm_t *self, void* data);
static void enter_press     (fsm_t *self, void* data);
static void enter_unpress   (fsm_t *self, void* data);
static void enter_long_press(fsm_t *self, void* data);
static void exit_press      (fsm_t *self, void* data);

// Define FSM states
FSM_STATES_INIT(btn_fsm)
//                  name  state id          parent           sub         entry             run     exit
FSM_CREATE_STATE(btn_fsm, ROOT_ST,          FSM_ST_NONE,   INIT_ST,      NULL,             NULL,   NULL)
FSM_CREATE_STATE(btn_fsm, INIT_ST,          ROOT_ST,       FSM_ST_NONE,  enter_init,       NULL,   NULL)
FSM_CREATE_STATE(btn_fsm, IDLE_ST,          ROOT_ST,       FSM_ST_NONE,  enter_idle,       NULL,   NULL)
FSM_CREATE_STATE(btn_fsm, PRESS_ST,         ROOT_ST,       WAIT_ST,      NULL,             NULL,   exit_press)
FSM_CREATE_STATE(btn_fsm, WAIT_ST,          PRESS_ST,      FSM_ST_NONE,  enter_wait,       NULL,   NULL)
FSM_CREATE_STATE(btn_fsm, S_PRESS_ST,       ROOT_ST,       FSM_ST_NONE,  enter_press,      NULL,   NULL)
FSM_CREATE_STATE(btn_fsm, S_UNPRESS_ST,     ROOT_ST,       FSM_ST_NONE,  enter_unpress,    NULL,   exit_press)
FSM_CREATE_STATE(btn_fsm, L_PRESS_ST,       PRESS_ST,      FSM_ST_NONE,  enter_long_press, NULL,   NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(btn_fsm)
//                    fsm name State source    event            state target
FSM_TRANSITION_CREATE(btn_fsm,   INIT_ST,      READY_EV,         IDLE_ST)
FSM_TRANSITION_CREATE(btn_fsm,   IDLE_ST,      PRESS_EV,         PRESS_ST)
FSM_TRANSITION_CREATE(btn_fsm,   PRESS_ST,     UNPRESS_EV,       IDLE_ST)
FSM_TRANSITION_CREATE(btn_fsm,   WAIT_ST,      FSM_TIMEOUT_EV,  S_PRESS_ST)
FSM_TRANSITION_CREATE(btn_fsm,   S_PRESS_ST,   FSM_TIMEOUT_EV,  L_PRESS_ST)
FSM_TRANSITION_CREATE(btn_fsm,   S_PRESS_ST,   UNPRESS_EV,      S_UNPRESS_ST)
FSM_TRANSITION_CREATE(btn_fsm,   S_UNPRESS_ST, READY_EV,        IDLE_ST)
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
static void gpio_isr_handler(void* arg)
{
    btn_ins_t * btn = (btn_ins_t *) arg;

    fsm_dispatch(&btn->fsm, (gpio_get_level(btn->gpio) == 0) ? PRESS_EV : UNPRESS_EV, btn);
}

static void enter_idle(fsm_t *self, void* data)
{
    btn_ins_t * btn = (btn_ins_t *) data;
    if(data == NULL) return;

    ESP_LOGI(TAG, "Init ready %d", (int)btn->gpio);
}

/**
 * @brief Internal task for running fsm every 10ms
 * 
 * @param arg 
 */
static void internal_task(void* arg)
{
    btn_ins_t * btn = (btn_ins_t *) arg;
    if(btn == NULL) 
    {
        ESP_LOGE(TAG, "Btn task error");
        vTaskDelete(NULL);
    }

    for(;;)
    {
        vTaskDelay(BTN_TASK_PERIOD_MS / portTICK_PERIOD_MS);

        btn_run(btn);
    }
}

/**
 * @brief Internal timer callback
 * 
 * @param xTimer 
 */
static void timed_events_cb(TimerHandle_t xTimer)
{
    fsm_t *fsm =  (fsm_t*)pvTimerGetTimerID(xTimer);

    if (fsm == NULL) {
        ESP_LOGE(TAG, "FSM instance is NULL in timer callback");
        return;
    }

    fsm_ticks_hook(fsm);
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
    btn_ins_t *btn = (btn_ins_t *) data;
    BaseType_t result;

    ESP_LOGI(TAG, "Enter init");

    if (btn == NULL) {
        ESP_LOGE(TAG, "Button instance is NULL");
        return;
    }

    ESP_LOGI(TAG, "Initializing button on GPIO %d", (int)btn->gpio);
    
    // GPIO init
    btn->io_conf.intr_type = GPIO_INTR_ANYEDGE;
    btn->io_conf.pin_bit_mask = (1ULL << btn->gpio);
    btn->io_conf.mode = GPIO_MODE_INPUT;
    btn->io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&btn->io_conf);
    
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
        return;
    }
    
    err = gpio_isr_handler_add(btn->gpio, gpio_isr_handler, data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(err));
        return;
    }
    
    // Queue init
    btn->evt_q = xQueueCreate(BTN_MAX_EVENTS, sizeof(btn_evt_t));
    if(btn->evt_q == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }
    
    ESP_LOGI(TAG, "Button queue init %d", (int)btn->evt_q);
    
    // Timer for timed events
    btn->timer = xTimerCreate(
        "Timer_btn",                                    // Timer name
        BTN_TIMER_PERIOD_MS / portTICK_PERIOD_MS,       // Period in ticks
        pdTRUE,                                         // Auto-reload (periodic)
        (void*)self,                                    // Timer ID (not used here)
        timed_events_cb                                   // Callback function
    );

    ESP_LOGI(TAG, "Timer init %d %d %d %d", (int)data, (int)self->current_data, (int)self->current_state, (int)self->transitions);
    
    if (!btn->timer)
    {
        ESP_LOGE(TAG, "Failed to create timer");
        return;
    } 

    if (xTimerStart(btn->timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer");
        return;
    }

    // Task init
    result = xTaskCreate(internal_task, "btn_task", 2048*6, (void*const)btn, tskIDLE_PRIORITY+5, NULL);
    if(result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task: %d", result);
        return;
    }

    ESP_LOGI(TAG, "Button initialization complete");

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

    ESP_LOGI(TAG, "Enter wait");
    
    btn->evt = BOUNCE_EV;
    // Sets timer target to antibounce time
    btn->max_count = BTN_ANTIBOUNCE_T;
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
    
    ESP_LOGI(TAG, "Enter press");
    
    // Sets event as pressed
    btn->evt = PRESSED_EV;

    // Sets timer target to long press time
    btn->max_count = BTN_LONG_PRESS_T;
}

/**
 * @brief Shor unpress event
 * 
 * @param self 
 * @param data 
 */
static void enter_unpress(fsm_t *self, void* data)
{
    fsm_dispatch(self, READY_EV, data);
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

    ESP_LOGI(TAG, "Enter long press");
    
    btn->evt = LONG_PRESS_EV;

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

    ESP_LOGI(TAG, "Exit press %d", (int)btn->evt);
    
    btn->internal_count = 0;

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

    fsm_init(&device->fsm, 
                FSM_TRANSITIONS_GET(btn_fsm), 
                FSM_TRANSITIONS_SIZE(btn_fsm), 
                LAST_EV, 
                BTN_TIMER_PERIOD_MS,
                &FSM_STATE_GET(btn_fsm, ROOT_ST), 
                device);

    fsm_timed_event_set(&FSM_STATE_GET(btn_fsm, WAIT_ST), BTN_ANTI_BOUNCE_MS);
    fsm_timed_event_set(&FSM_STATE_GET(btn_fsm, S_PRESS_ST), BTN_LONG_PRESS_MS);

    return 0;
}

/**
 * @brief Links actor to the button fsm
 * 
 * @param device 
 * @param actor 
 * @return int 
 */
int btn_actor_link(btn_ins_t *device, struct fsm_actor_t* actor, int actor_len)
{
    return fsm_actor_link(&device->fsm, actor, actor_len);
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
    
    if(device == NULL) return NO_EV;
    if(device->evt_q == NULL) return NO_EV;

    xQueueReceive(device->evt_q, &evt, maxWait);

    return evt;
} 