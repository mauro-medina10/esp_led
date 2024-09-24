
#include <string.h>

#include "led_strip.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "app_led.h"
#include "fsm.h"

static const char *TAG = "app_led";

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
    OFF_ST,
    ON_ST,
    UPDATE_ST,
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
};

// Action function prototypes
static void enter_init(fsm_t *self, void* data);
static void enter_on(fsm_t *self, void* data);
static void enter_off(fsm_t *self, void* data);
static void enter_update(fsm_t *self, void* data);

// Define FSM states
FSM_STATES_INIT(led_fsm)
//                  name  state id  parent           sub            entry           run   exit
FSM_CREATE_STATE(led_fsm, ROOT_ST,  FSM_ST_NONE,    INIT_ST,         NULL,          NULL, NULL)
FSM_CREATE_STATE(led_fsm, INIT_ST,  ROOT_ST,        FSM_ST_NONE,    enter_init,     NULL, NULL)
FSM_CREATE_STATE(led_fsm, OFF_ST,   ROOT_ST,        FSM_ST_NONE,    enter_off,      NULL, NULL)
FSM_CREATE_STATE(led_fsm, ON_ST,    ROOT_ST,        FSM_ST_NONE,    enter_on,       NULL, NULL)
FSM_CREATE_STATE(led_fsm, UPDATE_ST,ROOT_ST,        FSM_ST_NONE,    enter_update,   NULL, NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(led_fsm)
//                    fsm name    State source   event       state target
FSM_TRANSITION_CREATE(led_fsm,      INIT_ST,     READY_EV,      OFF_ST)
FSM_TRANSITION_CREATE(led_fsm,      OFF_ST,      ON_EV,         ON_ST)
FSM_TRANSITION_CREATE(led_fsm,      ON_ST,       OFF_EV,        OFF_ST)
FSM_TRANSITION_CREATE(led_fsm,      OFF_ST,      TOGGLE_EV,     ON_ST)
FSM_TRANSITION_CREATE(led_fsm,      ON_ST,       TOGGLE_EV,     OFF_ST)
FSM_TRANSITION_CREATE(led_fsm,      ON_ST,       UPDATE_EV,     UPDATE_ST)
FSM_TRANSITION_CREATE(led_fsm,      UPDATE_ST,   READY_EV,      ON_ST)
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
    
    ESP_LOGI(TAG, "LED init %d", led_data->strip_config.strip_gpio_num);
    
    ESP_ERROR_CHECK(led_strip_new_spi_device(&led_data->strip_config, &led_data->spi_config, &led_data->handle));

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

static void enter_update(fsm_t *self, void* data)
{
    led_ins_t *led_data = data;

    ESP_LOGI(TAG, "Updating colour %d", led_data->strip_config.strip_gpio_num);

    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    strip_update(led_data);
    
    /* Refresh the strip to send data */
    led_strip_refresh(led_data->handle);
    
    fsm_dispatch(self, READY_EV, data);
}

//------------------------------------------------------//
//  APP functions                                       //
//------------------------------------------------------//

/**
 * @brief toggles the led strip
 * 
 * @return uint8_t 
 */
void toggle_led(led_ins_t *device)
{
    fsm_dispatch(&device->led_fsm, TOGGLE_EV, device);
}

/**
 * @brief Configures the led strip
 * 
 */
void configure_led(led_ins_t *device)
{
    ESP_LOGI(TAG, "Inits the FSM %d", device->strip_config.strip_gpio_num);
    
    fsm_init(&device->led_fsm, FSM_TRANSITIONS_GET(led_fsm), FSM_TRANSITIONS_SIZE(led_fsm),
             &FSM_STATE_GET(led_fsm, ROOT_ST), device);
}

/**
 * @brief Runs led FSM
 * 
 * @return int 
 */
int app_led_run(led_ins_t *device)
{
    return fsm_run(&device->led_fsm); 
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
    if(index >= device->strip_config.max_leds) return -1;
    if(len > device->strip_config.max_leds) return -1;
    if((index+len) > device->strip_config.max_leds) return -1;
    if(fsm_state_get(&device->led_fsm) != ON_ST) return -2;

    memcpy(&device->colour[index], colour, sizeof(led_colour_t)*len);

    fsm_dispatch(&device->led_fsm, UPDATE_EV, device);

    return 0;
}