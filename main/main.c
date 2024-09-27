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

#include "app_led.h"
#include "app_btn.h"

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO
#define BUTTON_GPIO 0
#define LED_STRIP_GPIO 13

#define LONG_PRESS_LEN 8    // LONG_PRESS_LEN* 50mS

static const char *TAG = "main";

/**
 * @brief Button instance
 * 
 */
static btn_ins_t btn;

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

static void button_task(void* arg)
{
    btn_evt_t evt = 0;

    ESP_LOGI(TAG, "btn task %d", (int) arg);

    for(;;) {
        evt = btn_wait_for_event(&btn, portMAX_DELAY);

        if(evt == PRESSED_EV) {
            ESP_LOGI(TAG, "Button pressed");
            
            rotate_colour();
            app_led_update(&led_ext, 0, colour, 7);
        }else if(evt == LONG_PRESS_EV)
        {
            ESP_LOGI(TAG, "Button long pressed");

            toggle_led(&led);
            toggle_led(&led_ext);
        }
    }
}

static void btn_init(void)
{
    BaseType_t result;

    ESP_LOGI(TAG, "btn init %d", (int) &btn);

    btn_configure(&btn, BUTTON_GPIO);
    
    result = xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
    if(result != pdPASS)
    {
        ESP_LOGE(TAG, "Task error");
    }
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
    configure_led(&led);
    init_led_colour(&led_ext);
    configure_led(&led_ext);
    btn_init();

    while (1) {  
        app_led_run(&led);
        app_led_run(&led_ext);

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
