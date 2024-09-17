
#include "led_strip.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "app_led.h"
#include "fsm.h"

static const char *TAG = "app_led";

/**
 * @brief MEF states
 * 
 */
enum {
    ROOT_ST = 1,
    INIT_ST,
    OFF_ST,
    ON_ST,
};

/**
 * @brief MEF events
 * 
 */
enum {
    ON_EV = 0,
    OFF_EV,
    UPDATE_EV,
    READY_EV,
};

// Action function prototypes
static void enter_init(fsm_t *self, void* data);
static void enter_on(fsm_t *self, void* data);
static void enter_off(fsm_t *self, void* data);

// Define FSM states
FSM_STATES_INIT(rgb_led)
//                  name  state id  parent           sub            entry           run   exit
FSM_CREATE_STATE(rgb_led, ROOT_ST,  FSM_ST_NONE,    INIT_ST,         NULL,          NULL, NULL)
FSM_CREATE_STATE(rgb_led, INIT_ST,  ROOT_ST,        FSM_ST_NONE,    enter_init,     NULL, NULL)
FSM_CREATE_STATE(rgb_led, OFF_ST,   ROOT_ST,        FSM_ST_NONE,    enter_off,      NULL, NULL)
FSM_CREATE_STATE(rgb_led, ON_ST,    ROOT_ST,        FSM_ST_NONE,    enter_on,       NULL, NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(rgb_led)
//                    fsm name    State source   event       state target
FSM_TRANSITION_CREATE(rgb_led,      INIT_ST,     READY_EV,      OFF_ST)
FSM_TRANSITION_CREATE(rgb_led,      OFF_ST,      ON_EV,         ON_ST)
FSM_TRANSITION_CREATE(rgb_led,      ON_ST,       OFF_EV,        OFF_ST)
FSM_TRANSITION_CREATE(rgb_led,      ON_ST,       UPDATE_EV,     ON_ST)
FSM_TRANSITIONS_END()

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

static uint8_t s_led_state = 0;

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


/**
 * @brief LED data struct
 * 
 */
typedef struct
{
    led_strip_handle_t *handle;
    led_strip_config_t strip_config;
    led_strip_spi_config_t spi_config;

    uint32_t max_n_led;
    uint32_t index;
    led_colour_t colour; 
}led_ins_t;

static led_strip_handle_t led_strip;

static fsm_t rgb_led;

static led_ins_t led_device = {0};

// static led_ins_t led_devices[LED_MAX_STRIPS] = {0};

/**
 * @brief Initialice the led strip
 * 
 * @param self 
 * @param data 
 */
static void enter_init(fsm_t *self, void* data)
{
    ESP_LOGI(TAG, "LED init");
    
    /* LED strip initialization with the GPIO and pixels number*/
    led_device.handle = &led_strip;
    led_device.strip_config.strip_gpio_num = BLINK_GPIO;
    led_device.strip_config.max_leds = 1; // at least one LED on board
    led_device.spi_config.spi_bus = SPI2_HOST;
    led_device.spi_config.flags.with_dma = true;

    led_device.index = 0;
    led_device.colour.rgb.red = 200;
    led_device.colour.rgb.green = 16;
    led_device.colour.rgb.blue = 16;

    ESP_ERROR_CHECK(led_strip_new_spi_device(&led_device.strip_config, &led_device.spi_config, led_device.handle));

    fsm_dispatch(&rgb_led, READY_EV, &led_device);
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

    ESP_LOGI(TAG, "Turning on");

    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    led_strip_set_pixel(*led_data->handle, led_data->index, led_data->colour.rgb.red, led_data->colour.rgb.green, led_data->colour.rgb.blue);
    /* Refresh the strip to send data */
    led_strip_refresh(*led_data->handle);
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

    ESP_LOGI(TAG, "Turning off");

    /* Set all LED off to clear all pixels */
    led_strip_clear(*led_data->handle);
}

/**
 * @brief toggles the led strip
 * 
 * @return uint8_t 
 */
uint8_t blink_led(void)
{
    /* If the addressable LED is enabled */
    if (s_led_state) {

        fsm_dispatch(&rgb_led, ON_EV, &led_device);
    } else {
        /* Set all LED off to clear all pixels */
        fsm_dispatch(&rgb_led, OFF_EV, &led_device);
    }

    /* Toggle the LED state */
    s_led_state = !s_led_state;

    return s_led_state;
}

/**
 * @brief Configures the led strip
 * 
 */
void configure_led(void)
{
    ESP_LOGI(TAG, "Inits the FSM");
    
    fsm_init(&rgb_led, FSM_TRANSITIONS_GET(rgb_led), FSM_TRANSITIONS_SIZE(rgb_led),
             &FSM_STATE_GET(rgb_led, ROOT_ST), NULL);
}

void app_led_run(void)
{
    fsm_run(&rgb_led); 
}