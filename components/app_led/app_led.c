
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "led_strip.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "app_led.h"
#include "fsm.h"

static const char *TAG = "app_led";

//------------------------------------------------------//
//  FSM declarations                                    //
//------------------------------------------------------//
static void internal_led_task(void* arg);
static void timed_events_timer(TimerHandle_t xTimer);

/**
 * @brief MEF states
 * 
 */
enum {
    ROOT_ST = FSM_ST_FIRST,
    INIT_ST,
    OFF_ST,
    ON_ST,
    ON_FIX_ST,
    BLINKING_ST,
    BLINK_ON_ST,
    BLINK_OFF_ST,
    UPDATE_ST,
    LAST_ST,
};

/**
 * @brief MEF events
 * 
 */
enum {
    ON_EV = FSM_EV_FIRST,
    OFF_EV,
    UPDATE_EV,
    TOGGLE_EV,
    READY_EV,
    BLINK_EV,
    LAST_EV,
};

// Action function prototypes
static void enter_init(fsm_t *self, void* data);
static void enter_on(fsm_t *self, void* data);
static void enter_off(fsm_t *self, void* data);
static void led_update(fsm_t *self, void* data);

// Define FSM states
FSM_STATES_INIT(led_fsm)
//                  name  state id  parent           sub            entry           run   exit
FSM_CREATE_STATE(led_fsm, ROOT_ST,      FSM_ST_NONE,    INIT_ST,         NULL,          NULL, NULL)
FSM_CREATE_STATE(led_fsm, INIT_ST,      ROOT_ST,        FSM_ST_NONE,    enter_init,     NULL, NULL)
FSM_CREATE_STATE(led_fsm, OFF_ST,       ROOT_ST,        FSM_ST_NONE,    enter_off,      NULL, NULL)
FSM_CREATE_STATE(led_fsm, ON_ST,        ROOT_ST,        ON_FIX_ST,      enter_on,       NULL, NULL)
FSM_CREATE_STATE(led_fsm, ON_FIX_ST,    ON_ST,          FSM_ST_NONE,    enter_on,       NULL, NULL)
FSM_CREATE_STATE(led_fsm, BLINKING_ST,  ON_ST,          BLINK_OFF_ST,   NULL,           NULL, NULL)
FSM_CREATE_STATE(led_fsm, BLINK_ON_ST,  BLINKING_ST,    FSM_ST_NONE,    enter_on,       NULL, NULL)
FSM_CREATE_STATE(led_fsm, BLINK_OFF_ST, BLINKING_ST,    FSM_ST_NONE,    enter_off,      NULL, NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(led_fsm)
//                    fsm name    State source      event           state target
FSM_TRANSITION_CREATE(led_fsm,      INIT_ST,        READY_EV,       OFF_ST)
FSM_TRANSITION_CREATE(led_fsm,      OFF_ST,         ON_EV,          ON_ST)
FSM_TRANSITION_CREATE(led_fsm,      OFF_ST,         TOGGLE_EV,      ON_ST)
FSM_TRANSITION_CREATE(led_fsm,      ON_ST,          OFF_EV,         OFF_ST)
FSM_TRANSITION_CREATE(led_fsm,      ON_ST,          TOGGLE_EV,      OFF_ST)
FSM_TRANSITION_WORK_CREATE(led_fsm, ON_ST,          UPDATE_EV,       ON_ST,      led_update)
FSM_TRANSITION_CREATE(led_fsm,      ON_FIX_ST,      BLINK_EV,        BLINKING_ST)
FSM_TRANSITION_CREATE(led_fsm,      BLINK_ON_ST,    FSM_TIMEOUT_EV,  BLINK_OFF_ST)
FSM_TRANSITION_CREATE(led_fsm,      BLINK_OFF_ST,   FSM_TIMEOUT_EV,  BLINK_ON_ST)
FSM_TRANSITION_CREATE(led_fsm,      BLINKING_ST,    ON_EV,           ON_FIX_ST)
FSM_TRANSITION_CREATE(led_fsm,      BLINKING_ST,    BLINK_EV,        OFF_ST)
FSM_TRANSITIONS_END()

//------------------------------------------------------//
//  APP declarations                                    //
//------------------------------------------------------//


//------------------------------------------------------//
//  LOCAL functions                                     //
//------------------------------------------------------//
static void strip_update(led_ins_t *led_data)
{
    for (uint8_t i = 0; i < led_data->strip_config.max_leds; i++)
    {
        led_strip_set_pixel(led_data->handle, i, led_data->colour[i].rgb.red, led_data->colour[i].rgb.green, led_data->colour[i].rgb.blue);
    }
}

//------------------------------------------------------//
//  FSM functions                                       //
//------------------------------------------------------//
/**
 * @brief Initialice the led strip
 * 
 * @param self 
 * @param data 
 */
static void enter_init(fsm_t *self, void* data)
{
    led_ins_t *led_data = data;
    BaseType_t result;

    ESP_LOGI(TAG, "LED init %d", led_data->strip_config.strip_gpio_num);
    
    ESP_ERROR_CHECK(led_strip_new_spi_device(&led_data->strip_config, &led_data->spi_config, &led_data->handle));

    // Timer for timed events
    led_data->timer = xTimerCreate(
        "TimedEvents",                              // Timer name
        LED_TIMER_PERIOD_MS / portTICK_PERIOD_MS,   // Period in ticks
        pdTRUE,                                     // Auto-reload (periodic)
        (void*)self,                               // Timer ID (not used here)
        timed_events_timer                          // Callback function
    );

    if (led_data->timer != NULL) xTimerStart(led_data->timer, 0);
    
    // Task init
    result = xTaskCreate(internal_led_task, "led_task", 2048*2, (void*const)led_data, tskIDLE_PRIORITY+LED_TASK_PRIOR, NULL);
    if(result != pdPASS)
    {
        ESP_LOGE(TAG, "Task error");
        return;
    }

    fsm_dispatch(self, READY_EV, data);
}

/**
 * @brief Turns the led on
 * 
 * @param self 
 * @param data 
 */
static void enter_on(fsm_t *self, void* data)
{
    led_ins_t *led_data = data;

    ESP_LOGI(TAG, "Turning on %d", led_data->strip_config.strip_gpio_num);

    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    strip_update(led_data);
    /* Refresh the strip to send data */
    led_strip_refresh(led_data->handle);
}

/**
 * @brief Turns the led off
 * 
 * @param self 
 * @param data 
 */
static void enter_off(fsm_t *self, void* data)
{
    led_ins_t *led_data = data;

    ESP_LOGI(TAG, "Turning off %d", led_data->strip_config.strip_gpio_num);

    /* Set all LED off to clear all pixels */
    led_strip_clear(led_data->handle);
}

static void led_update(fsm_t *self, void* data)
{
    led_ins_t *led_data = data;

    ESP_LOGI(TAG, "Updating colour %d", led_data->strip_config.strip_gpio_num);

    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    strip_update(led_data);
    
    /* Refresh the strip to send data */
    led_strip_refresh(led_data->handle);
}

/**
 * @brief Internal task for running fsm every 10ms
 * 
 * @param arg 
 */
static void internal_led_task(void* arg)
{
    led_ins_t * led = (led_ins_t *) arg;
    if(led == NULL) 
    {
        ESP_LOGE(TAG, "Led task error");
        vTaskDelete(NULL);
    }

    for(;;)
    {
        vTaskDelay(LED_TASK_PERIOD_MS / portTICK_PERIOD_MS);

        app_led_run(led);
    }
}

// Timer callback function
static void timed_events_timer(TimerHandle_t xTimer)
{
    fsm_t *fsm =  (fsm_t*)pvTimerGetTimerID(xTimer);

    fsm_ticks_hook(fsm);
}

//------------------------------------------------------//
//  APP functions                                       //
//------------------------------------------------------//

/**
 * @brief toggles the led strip
 * 
 * @return uint8_t 
 */
void blink_led(led_ins_t *device)
{
    if(device == NULL) return;

    fsm_dispatch(&device->fsm, BLINK_EV, device);
}

/**
 * @brief toggles the led strip
 * 
 * @return uint8_t 
 */
void toggle_led(led_ins_t *device)
{
    if(device == NULL) return;

    fsm_dispatch(&device->fsm, TOGGLE_EV, device);
}


void led_on(led_ins_t *device)
{
    if(device == NULL) return;

    fsm_dispatch(&device->fsm, ON_EV, device);
}

void led_off(led_ins_t *device)
{
    if(device == NULL) return;

    fsm_dispatch(&device->fsm, OFF_EV, device);
}

/**
 * @brief Configures the led strip
 * 
 */
void configure_led(led_ins_t *device)
{
    if(device == NULL) return;

    ESP_LOGI(TAG, "Inits the FSM %d", device->strip_config.strip_gpio_num);
    
    fsm_init(&device->fsm, 
                FSM_TRANSITIONS_GET(led_fsm), 
                FSM_TRANSITIONS_SIZE(led_fsm), 
                LAST_EV, 
                LED_TIMER_PERIOD_MS,
                &FSM_STATE_GET(led_fsm, ROOT_ST), 
                device);

    fsm_timed_event_set(&FSM_STATE_GET(led_fsm, BLINK_ON_ST), LED_BLINK_PERIOD);
    fsm_timed_event_set(&FSM_STATE_GET(led_fsm, BLINK_OFF_ST), LED_BLINK_PERIOD);
}

/**
 * @brief Runs led FSM
 * 
 * @return int 
 */
int app_led_run(led_ins_t *device)
{
    if(device == NULL) return -1;

    return fsm_run(&device->fsm); 
}

/**
 * @brief  Changes led colour
 * 
 * @param device 
 * @param index 
 * @param colour 
 * @param len number of led to update
 * @return int 
 */
int app_led_update(led_ins_t *device, uint32_t index, led_colour_t *colour, uint32_t len)
{
    if(device == NULL || colour == NULL) return -11;
    if(index >= device->strip_config.max_leds) return -12;
    if(len == 0 || len > device->strip_config.max_leds) return -13;
    if((index+len) > device->strip_config.max_leds) return -14;
    
    int ret = fsm_state_get(&device->fsm);
    
    if(ret != ON_FIX_ST) return -ret;
    
    memcpy(&device->colour[index], colour, sizeof(led_colour_t)*len);

    fsm_dispatch(&device->fsm, UPDATE_EV, device);

    return 0;
}