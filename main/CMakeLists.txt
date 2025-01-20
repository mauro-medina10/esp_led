idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES app_led app_btn fsm fsm_music espressif__led_strip driver)
