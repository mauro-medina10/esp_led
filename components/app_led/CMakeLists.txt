idf_component_register(SRCS "app_led.c"
                       INCLUDE_DIRS "include"
                       REQUIRES fsm espressif__led_strip driver)
