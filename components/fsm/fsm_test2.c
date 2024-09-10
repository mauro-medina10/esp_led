/**
 * @file fsm_test2.c
 * @author Mauro Medina (mauro.medina@kretz.com.ar)
 * @brief 
 * @version 1.0.1
 * @date 2024-07-17
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifdef CUSTOM_MAIN
#include "SRC/Testing/test_conf.h"

#ifdef FSM_TEST_2
#include <stdio.h>
#include <stdlib.h>

#include <atmel_start.h>
#include "../Drivers/Drv_TC.h"
#include "fsm.h"

/* LOGGING MODULE REGISTER */
#include "SRC/RTT/rtt_log.h"
/// @brief Terminal RTT de logeo
#define FSM_TEST_RTT_TERMINAL 3	
/// @brief Nivel de Debug para logeo
#define FSM_TEST_RTT_LEVEL 2		

RTT_LOG_REGISTER(fsm_test, FSM_TEST_RTT_LEVEL, FSM_TEST_RTT_TERMINAL);

// Define states
enum {
    ST_ROOT = 1,            // 1 
    ST_OFF,                 // 2
    ST_ON,                  // 3
    ST_PLAYING,             // 4
    ST_NORMAL,              // 5
    ST_SHUFFLE,             // 6
    ST_REPEAT,              // 7
    ST_PAUSED,              // 8
    ST_MENU,                // 9
    ST_VOLUME_ADJUST,       // 10
    ST_PLAYLIST_SELECT,     // 11
    ST_LOW_BATTERY          // 12
};

// Define events
enum {
    EV_POWER = 0,       // 0
    EV_PLAY,            // 1
    EV_PAUSE,           // 2
    EV_STOP,            // 3
    EV_NEXT,            // 4
    EV_PREV,            // 5
    EV_MODE_CHANGE,     // 6
    EV_MENU,            // 7
    EV_VOLUME_UP,       // 8
    EV_VOLUME_DOWN,     // 9
    EV_SELECT,          // 10
    EV_BACK,            // 11
    EV_LOW_BATTERY,     // 12
    EV_CHARGE           // 13
};

// Action function prototypes
static void enter_root(fsm_t *self, void* data);
static void enter_off(fsm_t *self, void* data);
static void enter_on(fsm_t *self, void* data);
static void enter_playing(fsm_t *self, void* data);
static void enter_normal(fsm_t *self, void* data);
static void enter_shuffle(fsm_t *self, void* data);
static void enter_repeat(fsm_t *self, void* data);
static void enter_paused(fsm_t *self, void* data);
static void enter_menu(fsm_t *self, void* data);
static void enter_volume_adjust(fsm_t *self, void* data);
static void enter_playlist_select(fsm_t *self, void* data);
static void enter_low_battery(fsm_t *self, void* data);

static void run_root(fsm_t* self, void* data);
static void run_off(fsm_t* self, void* data);
static void run_on(fsm_t* self, void* data);
static void run_playing(fsm_t* self, void* data);
static void run_normal(fsm_t* self, void* data);
static void run_shuffle(fsm_t* self, void* data);
static void run_repeat(fsm_t* self, void* data);
static void run_paused(fsm_t* self, void* data);
static void run_menu(fsm_t* self, void* data);
static void run_volume_adjust(fsm_t* self, void* data);
static void run_playlist_select(fsm_t* self, void* data);
static void run_low_battery(fsm_t* self, void* data);

// Define FSM states
FSM_STATES_INIT(music_player)
FSM_CREATE_STATE(music_player, ST_ROOT,             FSM_ST_NONE, ST_OFF,      enter_root,            run_root, NULL)
FSM_CREATE_STATE(music_player, ST_OFF,              ST_ROOT,     FSM_ST_NONE, enter_off,             run_off, NULL)
FSM_CREATE_STATE(music_player, ST_ON,               ST_ROOT,     ST_PAUSED,   enter_on,              run_on, NULL)
FSM_CREATE_STATE(music_player, ST_PLAYING,          ST_ON,       ST_NORMAL,   enter_playing,         run_playing, NULL)
FSM_CREATE_STATE(music_player, ST_NORMAL,           ST_PLAYING,  FSM_ST_NONE, enter_normal,          run_normal, NULL)
FSM_CREATE_STATE(music_player, ST_SHUFFLE,          ST_PLAYING,  FSM_ST_NONE, enter_shuffle,         run_shuffle, NULL)
FSM_CREATE_STATE(music_player, ST_REPEAT,           ST_PLAYING,  FSM_ST_NONE, enter_repeat,          run_repeat, NULL)
FSM_CREATE_STATE(music_player, ST_PAUSED,           ST_ON,       FSM_ST_NONE, enter_paused,          run_paused, NULL)
FSM_CREATE_STATE(music_player, ST_MENU,             ST_ON,       FSM_ST_NONE, enter_menu,            run_menu, NULL)
FSM_CREATE_STATE(music_player, ST_VOLUME_ADJUST,    ST_MENU,     FSM_ST_NONE, enter_volume_adjust,   run_volume_adjust, NULL)
FSM_CREATE_STATE(music_player, ST_PLAYLIST_SELECT,  ST_MENU,     FSM_ST_NONE, enter_playlist_select, run_playlist_select, NULL)
FSM_CREATE_STATE(music_player, ST_LOW_BATTERY,      ST_ROOT,     FSM_ST_NONE, enter_low_battery,     run_low_battery, NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(music_player)
FSM_TRANSITION_CREATE(music_player, ST_OFF,             EV_POWER,       ST_ON)
FSM_TRANSITION_CREATE(music_player, ST_ON,              EV_POWER,       ST_OFF)
FSM_TRANSITION_CREATE(music_player, ST_PAUSED,          EV_PLAY,        ST_PLAYING)
FSM_TRANSITION_CREATE(music_player, ST_PLAYING,         EV_PAUSE,       ST_PAUSED)
FSM_TRANSITION_CREATE(music_player, ST_PLAYING,         EV_STOP,        ST_PAUSED)
FSM_TRANSITION_CREATE(music_player, ST_NORMAL,          EV_MODE_CHANGE, ST_SHUFFLE)
FSM_TRANSITION_CREATE(music_player, ST_SHUFFLE,         EV_MODE_CHANGE, ST_REPEAT)
FSM_TRANSITION_CREATE(music_player, ST_REPEAT,          EV_MODE_CHANGE, ST_NORMAL)
FSM_TRANSITION_CREATE(music_player, ST_ON,              EV_MENU,        ST_MENU)
FSM_TRANSITION_CREATE(music_player, ST_MENU,            EV_BACK,        ST_ON)
FSM_TRANSITION_CREATE(music_player, ST_MENU,            EV_VOLUME_UP,   ST_VOLUME_ADJUST)
FSM_TRANSITION_CREATE(music_player, ST_MENU,            EV_VOLUME_DOWN, ST_VOLUME_ADJUST)
FSM_TRANSITION_CREATE(music_player, ST_VOLUME_ADJUST,   EV_BACK,        ST_MENU)
FSM_TRANSITION_CREATE(music_player, ST_MENU,            EV_SELECT,      ST_PLAYLIST_SELECT)
FSM_TRANSITION_CREATE(music_player, ST_PLAYLIST_SELECT, EV_BACK,        ST_MENU)
FSM_TRANSITION_CREATE(music_player, ST_ROOT,            EV_LOW_BATTERY, ST_LOW_BATTERY)
FSM_TRANSITION_CREATE(music_player, ST_LOW_BATTERY,     EV_CHARGE,      ST_ON)
FSM_TRANSITIONS_END()

// Action function implementations
static void enter_root(fsm_t *self, void* data) { RTT_LOG("Entering ROOT state\n"); }
static void enter_off(fsm_t *self, void* data) { RTT_LOG("Entering OFF state\n"); }
static void enter_on(fsm_t *self, void* data) { RTT_LOG("Entering ON state\n"); }
static void enter_playing(fsm_t *self, void* data) { RTT_LOG("Entering PLAYING state\n"); }
static void enter_normal(fsm_t *self, void* data) { RTT_LOG("Entering NORMAL play state\n"); }
static void enter_shuffle(fsm_t *self, void* data) { RTT_LOG("Entering SHUFFLE play state\n"); }
static void enter_repeat(fsm_t *self, void* data) { RTT_LOG("Entering REPEAT play state\n"); }
static void enter_paused(fsm_t *self, void* data) { RTT_LOG("Entering PAUSED state\n"); }
static void enter_menu(fsm_t *self, void* data) { RTT_LOG("Entering MENU state\n"); }
static void enter_volume_adjust(fsm_t *self, void* data) { RTT_LOG("Entering VOLUME ADJUST state\n"); }
static void enter_playlist_select(fsm_t *self, void* data) { RTT_LOG("Entering PLAYLIST SELECT state\n"); }
static void enter_low_battery(fsm_t *self, void* data) { RTT_LOG("Entering LOW BATTERY state\n"); }

static void run_root(fsm_t* self, void* data) { RTT_LOG("Running ROOT state\n"); }
static void run_off(fsm_t* self, void* data) { RTT_LOG("Music player is OFF\n"); }
static void run_on(fsm_t* self, void* data) { RTT_LOG("Music player is ON\n"); }
static void run_playing(fsm_t* self, void* data) { RTT_LOG("Music is playing\n"); }
static void run_normal(fsm_t* self, void* data) { RTT_LOG("Playing in NORMAL mode\n"); }
static void run_shuffle(fsm_t* self, void* data) { RTT_LOG("Playing in SHUFFLE mode\n"); }
static void run_repeat(fsm_t* self, void* data) { RTT_LOG("Playing in REPEAT mode\n"); }
static void run_paused(fsm_t* self, void* data) { RTT_LOG("Music is PAUSED\n"); }
static void run_menu(fsm_t* self, void* data) { RTT_LOG("In MENU\n"); }
static void run_volume_adjust(fsm_t* self, void* data) { RTT_LOG("Adjusting VOLUME\n"); }
static void run_playlist_select(fsm_t* self, void* data) { RTT_LOG("Selecting PLAYLIST\n"); }
static void run_low_battery(fsm_t* self, void* data) { RTT_LOG("LOW BATTERY warning\n"); }

/**
 * @brief Reinicia el sistema en caso de HF
 * 
 */
void HardFault_Handler(void)
{
	
	RTT_ERROR("Hard Fault Reset... \n");
	
	RTT_WAIT_LOG(1000);

	NVIC_SystemReset();
}

int main() {
    fsm_t music_player;
    int ret = 0;

    // Initializes MCU, drivers and middleware 
	atmel_start_init();	

    //Timer init	
	iniciar_timer1(TIMER_1_CONFIG_UC); 

    // Simulate music player actions
    RTT_LOG("--- Starting Complex Music Player Simulation ---\n");

    fsm_init(&music_player, FSM_TRANSITIONS_GET(music_player), FSM_TRANSITIONS_SIZE(music_player),
             &FSM_STATE_GET(music_player, ST_ROOT), NULL);

    ret |= fsm_run(&music_player);  // Should be in OFF state (default substate of ROOT)
    
    espera_ms(10);

    fsm_dispatch(&music_player, EV_POWER, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> PAUSED state
    RTT_LOG("Turning on the player... %s\n", RTT_CHECK(fsm_state_get(&music_player) == ST_PAUSED));

    espera_ms(10);
    
    fsm_dispatch(&music_player, EV_PLAY, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> PLAYING -> NORMAL state
    RTT_LOG("Starting playback... %s\n", RTT_CHECK(fsm_state_get(&music_player) == ST_NORMAL));

    espera_ms(10);
    
    fsm_dispatch(&music_player, EV_MODE_CHANGE, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> PLAYING -> SHUFFLE state
    RTT_LOG("Changing play mode... %s\n", RTT_CHECK(fsm_state_get(&music_player) == ST_SHUFFLE));

    espera_ms(10);
    
    fsm_dispatch(&music_player, EV_MENU, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> MENU state
    RTT_LOG("Opening menu... %s\n", RTT_CHECK(fsm_state_get(&music_player) == ST_MENU));

    espera_ms(10);
    
    fsm_dispatch(&music_player, EV_VOLUME_UP, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> MENU -> VOLUME_ADJUST state
    RTT_LOG("Adjusting volume... %s\n", RTT_CHECK(fsm_state_get(&music_player) == ST_VOLUME_ADJUST));

    espera_ms(10);
    RTT_WAIT_LOG(10000);

    fsm_dispatch(&music_player, EV_BACK, NULL);
    ret |= fsm_run(&music_player);  // Should be back in ON -> MENU state
    RTT_LOG("Going back to menu... %s\n", RTT_CHECK(fsm_state_get(&music_player) == ST_MENU));

    espera_ms(10);
    
    fsm_dispatch(&music_player, EV_SELECT, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> MENU -> PLAYLIST_SELECT state
    RTT_LOG("Selecting playlist... %s\n", RTT_CHECK(fsm_state_get(&music_player) == ST_PLAYLIST_SELECT));

    espera_ms(10);
    
    fsm_dispatch(&music_player, EV_LOW_BATTERY, NULL);
    ret |= fsm_run(&music_player);  // Should transition to LOW_BATTERY state
    RTT_LOG("Low battery event... %s\n", RTT_CHECK(fsm_state_get(&music_player) == ST_LOW_BATTERY));

    espera_ms(10);
    
    fsm_dispatch(&music_player, EV_CHARGE, NULL);
    ret |= fsm_run(&music_player);  // Should transition back to ON state
    RTT_LOG("Charging the player... %s\n", RTT_CHECK(fsm_state_get(&music_player) == ST_PAUSED));

    espera_ms(10);
    
    fsm_dispatch(&music_player, EV_POWER, NULL);
    ret |= fsm_run(&music_player);  // Should be in OFF state
    RTT_LOG("Turning off the player... %s\n", RTT_CHECK(fsm_state_get(&music_player) == ST_OFF));

    espera_ms(10);
    
    RTT_LOG("--- End of Complex Music Player Simulation %s---\n", RTT_CHECK(ret == 0));

    while (1)
    {
        RTT_SYSTEM_RESET();
        espera_ms(200);
    }

    return 0;
}
#endif
#endif