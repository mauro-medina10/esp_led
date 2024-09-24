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

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO
#define BUTTON_GPIO 0
#define LED_STRIP_GPIO 13

#define LONG_PRESS_LEN 8    // LONG_PRESS_LEN* 50mS

static const char *TAG = "main";

static QueueHandle_t gpio_evt_queue = NULL;
static led_colour_t colour[7];

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

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

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

static int long_press_check(void)
{
    int aux = 0;

    for (uint8_t i = 0; i < LONG_PRESS_LEN; i++)
    {
        vTaskDelay((50 / portTICK_PERIOD_MS));
        if(gpio_get_level(BUTTON_GPIO) == 1)
        {
            aux = 1;
            break;
        } 
        aux = 0;
    }

    return aux;
}

static void button_task(void* arg)
{
    uint32_t io_num;
    int aux = 0;

    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Button pressed");
            
            // Check for long press
            aux = long_press_check();

            if(aux == 0)    // Long press
            {
                toggle_led(&led);
                toggle_led(&led_ext);
            }else
            {     
                rotate_colour();

                app_led_update(&led_ext, 0, colour, 7);
            }
        }
    }
}

static void button_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void*) BUTTON_GPIO);

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
    button_init();

    while (1) {        
        app_led_run(&led);
        app_led_run(&led_ext);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
