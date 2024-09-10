idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES app_led fsm espressif__led_strip driver)
