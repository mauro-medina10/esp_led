/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "fsm.h"

#include "app_led.h"
#include "app_btn.h"

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO
#define BUTTON_GPIO 0
// #define LED_STRIP_GPIO 13

#define LONG_PRESS_LEN 8    // LONG_PRESS_LEN* 50mS

static const char *TAG = "main";

/**
 * @brief Button instance
 * 
 */
static btn_ins_t btn;

static void btn_pressed(fsm_t* self, void* data);
static void btn_long_pressed(fsm_t* self, void* data);

// Button Actor 
FSM_ACTOR_INIT(btn_actor)
FSM_ACTOR_CREATE(S_PRESS_ST, btn_pressed, NULL, NULL)
FSM_ACTOR_CREATE(L_PRESS_ST, btn_long_pressed, NULL, NULL)
FSM_ACTOR_END()

/**
 * @brief Internal LED instance definition
 * 
 */
static led_ins_t led = {
    .strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1,
    },
    .spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    },
    .colour[0] = {
        .rgb.red = 200,
        .rgb.green = 16,
        .rgb.blue = 16,
    }
};

static led_colour_t colour[7];

#ifdef LED_STRIP_GPIO
/**
 * @brief External LED instance definition
 * 
 */
static led_ins_t led_ext = {
    .strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = 7,
    },
    .spi_config = {
        .spi_bus = SPI3_HOST,
        .flags.with_dma = true,
    },
};
#endif
static void rotate_colour(void)
{
    uint32_t aux = 0;

    for (size_t i = 0; i < 7; i++)
    {
        aux = colour[i].rgb.red;
        
        colour[i].rgb.red = colour[i].rgb.green;
        colour[i].rgb.green = colour[i].rgb.blue;
        colour[i].rgb.blue = aux;
    }
}
#ifdef CONFIG_CUSTOM_BTN_TASK
static void button_task(void* arg)
{
    btn_evt_t evt = 0;

    ESP_LOGI(TAG, "btn task %d", (int) arg);

    for(;;) {
        evt = btn_wait_for_event(&btn, portMAX_DELAY);

        if(evt == PRESSED_EV) {
            ESP_LOGI(TAG, "Button pressed");
            
            rotate_colour();       
            app_led_update(&led, 0, colour, led.strip_config.max_leds);         
        }else if(evt == LONG_PRESS_EV)
        {
            ESP_LOGI(TAG, "Button long pressed");

            toggle_led(&led);
            fsm_music();
#ifdef LED_STRIP_GPIO            
            toggle_led(&led_ext);
#endif            
        }else
        {
            ESP_LOGI(TAG, "Button wrong ev %d", (int)evt);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}
#else
static void btn_pressed(fsm_t* self, void* data)
{
    rotate_colour();       
    app_led_update(&led, 0, colour, led.strip_config.max_leds);   
}

static void btn_long_pressed(fsm_t* self, void* data)
{
    toggle_led(&led);
}
#endif

static void btn_init(void)
{
    btn_configure(&btn, BUTTON_GPIO);

#ifdef CONFIG_CUSTOM_BTN_TASK
    BaseType_t result;

    result = xTaskCreate(button_task, "button_task", 2048*4, NULL, 10, NULL);
    if(result != pdPASS)
    {
        ESP_LOGE(TAG, "Task error");
    }
#else    
    btn_actor_link(&btn, FSM_ACTOR_GET(btn_actor), FSM_ACTOR_SIZE(btn_actor));
#endif
}

static void init_led_colour(led_ins_t *led)
{
    for (size_t i = 0; i < led->strip_config.max_leds; i++)
    {
        led->colour[i].rgb.red = 16;
        led->colour[i].rgb.green = 16;
        led->colour[i].rgb.blue = 200;
        colour[i].rgb.red = 16;
        colour[i].rgb.green = 16;
        colour[i].rgb.blue = 200;
    }
}

void app_main(void)
{
    /* Configure the peripheral according to the LED type */
    init_led_colour(&led);
    configure_led(&led);
#ifdef LED_STRIP_GPIO    
    configure_led(&led_ext);
#endif
    btn_init();

    ESP_LOGI(TAG,"End main task");

    vTaskDelete(NULL);
}
