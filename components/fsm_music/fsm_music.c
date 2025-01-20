/**
 * @file fsm_music.c
 * @author Mauro Medina 
 * @brief 
 * @version 1.0.1
 * @date 2024-07-17
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fsm.h"
#include "fsm_music.h"
#include "esp_log.h"

#ifndef LOG_CHECK
#define LOG_CHECK(val) ((val) == 1 ? "OK" : "ERROR")
#endif

static const char *TAG = "music";
static const char *TAG_e = "music_enter";
static const char *TAG_r = "music_run";

// Define states
enum {
    ST_ROOT = FSM_ST_FIRST,            // 1 
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
    EV_POWER = 2,       // 0
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
    EV_CHARGE,          // 13
    EV_LAST,
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

// Actor 1
static void act1_enter_on(fsm_t *self, void* data);
static void act1_enter_shuffle(fsm_t *self, void* data);
static void act1_enter_menu(fsm_t *self, void* data);
static void act1_enter_low_battery(fsm_t *self, void* data);
static void act1_run_on(fsm_t* self, void* data);
static void act1_run_shuffle(fsm_t* self, void* data);
static void act1_run_menu(fsm_t* self, void* data);
static void act1_run_low_battery(fsm_t* self, void* data);

// Actor 2
static void act2_enter_on(fsm_t *self, void* data);
static void act2_enter_shuffle(fsm_t *self, void* data);
static void act2_enter_menu(fsm_t *self, void* data);
static void act2_enter_low_battery(fsm_t *self, void* data);
static void act2_run_on(fsm_t* self, void* data);
static void act2_run_shuffle(fsm_t* self, void* data);
static void act2_run_menu(fsm_t* self, void* data);
static void act2_run_low_battery(fsm_t* self, void* data);

// Define FSM states
FSM_STATES_INIT(music_player)
//                  name        state id          parent           sub          entry                 run                   exit
FSM_CREATE_STATE(music_player, ST_ROOT,             FSM_ST_NONE, ST_OFF,      enter_root,            run_root,              NULL)
FSM_CREATE_STATE(music_player, ST_OFF,              ST_ROOT,     FSM_ST_NONE, enter_off,             run_off,               NULL)
FSM_CREATE_STATE(music_player, ST_ON,               ST_ROOT,     ST_PAUSED,   enter_on,              run_on,                NULL)
FSM_CREATE_STATE(music_player, ST_PLAYING,          ST_ON,       ST_NORMAL,   enter_playing,         run_playing,           NULL)
FSM_CREATE_STATE(music_player, ST_NORMAL,           ST_PLAYING,  FSM_ST_NONE, enter_normal,          run_normal,            NULL)
FSM_CREATE_STATE(music_player, ST_SHUFFLE,          ST_PLAYING,  FSM_ST_NONE, enter_shuffle,         run_shuffle,           NULL)
FSM_CREATE_STATE(music_player, ST_REPEAT,           ST_PLAYING,  FSM_ST_NONE, enter_repeat,          run_repeat,            NULL)
FSM_CREATE_STATE(music_player, ST_PAUSED,           ST_ON,       FSM_ST_NONE, enter_paused,          run_paused,            NULL)
FSM_CREATE_STATE(music_player, ST_MENU,             ST_ON,       FSM_ST_NONE, enter_menu,            run_menu,              NULL)
FSM_CREATE_STATE(music_player, ST_VOLUME_ADJUST,    ST_MENU,     FSM_ST_NONE, enter_volume_adjust,   run_volume_adjust,     NULL)
FSM_CREATE_STATE(music_player, ST_PLAYLIST_SELECT,  ST_MENU,     FSM_ST_NONE, enter_playlist_select, run_playlist_select,   NULL)
FSM_CREATE_STATE(music_player, ST_LOW_BATTERY,      ST_ROOT,     FSM_ST_NONE, enter_low_battery,     run_low_battery,       NULL)
FSM_STATES_END()

// Define FSM transitions
FSM_TRANSITIONS_INIT(music_player)
//                      fsm name    State source        event       state target
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

// Actor 1
FSM_ACTOR_INIT(actor1)
FSM_ACTOR_CREATE(music_player, ST_ON, act1_enter_on, act1_run_on, NULL)
FSM_ACTOR_CREATE(music_player, ST_SHUFFLE, act1_enter_shuffle, act1_run_shuffle, NULL)
FSM_ACTOR_CREATE(music_player, ST_MENU, act1_enter_menu, act1_run_menu, NULL)
FSM_ACTOR_CREATE(music_player, ST_LOW_BATTERY, act1_enter_low_battery, act1_run_low_battery, NULL)
FSM_ACTOR_END()

// Actor 1
FSM_ACTOR_INIT(actor2)
FSM_ACTOR_CREATE(music_player, ST_ON, act2_enter_on, act2_run_on, NULL)
FSM_ACTOR_CREATE(music_player, ST_SHUFFLE, act2_enter_shuffle, act2_run_shuffle, NULL)
FSM_ACTOR_CREATE(music_player, ST_MENU, act2_enter_menu, act2_run_menu, NULL)
FSM_ACTOR_CREATE(music_player, ST_LOW_BATTERY, act2_enter_low_battery, act2_run_low_battery, NULL)
FSM_ACTOR_END()

// Action function implementations
static void enter_root(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering ROOT state"); }
static void enter_off(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering OFF state"); }
static void enter_on(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering ON state"); }
static void enter_playing(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering PLAYING state"); }
static void enter_normal(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering NORMAL play state"); }
static void enter_shuffle(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering SHUFFLE play state"); }
static void enter_repeat(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering REPEAT play state"); }
static void enter_paused(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering PAUSED state"); }
static void enter_menu(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering MENU state"); }
static void enter_volume_adjust(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering VOLUME ADJUST state"); }
static void enter_playlist_select(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering PLAYLIST SELECT state"); }
static void enter_low_battery(fsm_t *self, void* data) { ESP_LOGI(TAG_e, "Entering LOW BATTERY state"); }

static void run_root(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "Running ROOT state"); }
static void run_off(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "Music player is OFF"); }
static void run_on(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "Music player is ON"); }
static void run_playing(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "Music is playing"); }
static void run_normal(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "Playing in NORMAL mode"); }
static void run_shuffle(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "Playing in SHUFFLE mode"); }
static void run_repeat(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "Playing in REPEAT mode"); }
static void run_paused(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "Music is PAUSED"); }
static void run_menu(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "In MENU"); }
static void run_volume_adjust(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "Adjusting VOLUME"); }
static void run_playlist_select(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "Selecting PLAYLIST"); }
static void run_low_battery(fsm_t* self, void* data) { ESP_LOGI(TAG_r, "LOW BATTERY warning"); }

// Actor 1
static void act1_enter_on(fsm_t *self, void* data){ESP_LOGI(TAG, "act1 enter on");}
static void act1_enter_shuffle(fsm_t *self, void* data){ESP_LOGI(TAG, "act1 enter shuff");}
static void act1_enter_menu(fsm_t *self, void* data){ESP_LOGI(TAG, "act1 enter menu");}
static void act1_enter_low_battery(fsm_t *self, void* data){ESP_LOGI(TAG, "act1 enter low");}
static void act1_run_on(fsm_t* self, void* data){ESP_LOGI(TAG, "act1 run on");}
static void act1_run_shuffle(fsm_t* self, void* data){ESP_LOGI(TAG, "act1 run shuff");}
static void act1_run_menu(fsm_t* self, void* data){ESP_LOGI(TAG, "act1 run menu");}
static void act1_run_low_battery(fsm_t* self, void* data){ESP_LOGI(TAG, "act1 run low");}

// Actor 2
static void act2_enter_on(fsm_t *self, void* data){ESP_LOGI(TAG, "act2 enter on");}
static void act2_enter_shuffle(fsm_t *self, void* data){ESP_LOGI(TAG, "act2 enter shuff");}
static void act2_enter_menu(fsm_t *self, void* data){ESP_LOGI(TAG, "act2 enter menu");}
static void act2_enter_low_battery(fsm_t *self, void* data){ESP_LOGI(TAG, "act2 enter low");}
static void act2_run_on(fsm_t* self, void* data){ESP_LOGI(TAG, "act2 run on");}
static void act2_run_shuffle(fsm_t* self, void* data){ESP_LOGI(TAG, "act2 run shuff");}
static void act2_run_menu(fsm_t* self, void* data){ESP_LOGI(TAG, "act2 run menu");}
static void act2_run_low_battery(fsm_t* self, void* data){ESP_LOGI(TAG, "act2 run low");}

int fsm_music(void) {
    fsm_t music_player;
    int ret = 0;

    // Simulate music player actions
    ESP_LOGI(TAG, "--- Starting Complex Music Player Simulation ---\n");

    fsm_init(&music_player, FSM_TRANSITIONS_GET(music_player), FSM_TRANSITIONS_SIZE(music_player), EV_LAST,
             &FSM_STATE_GET(music_player, ST_ROOT), NULL);

    ret |= fsm_actor_link(&music_player, FSM_ACTOR_GET(actor1));
    ESP_LOGI(TAG, "Actor 1 link: %s", LOG_CHECK((ret == 0)));
    ret |= fsm_actor_link(&music_player, FSM_ACTOR_GET(actor2));
    ESP_LOGI(TAG, "Actor 2 link: %s", LOG_CHECK((ret == 0)));

    ret |= fsm_run(&music_player);  // Should be in OFF state (default substate of ROOT)

    vTaskDelay(10 / portTICK_PERIOD_MS);

    fsm_dispatch(&music_player, EV_POWER, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> PAUSED state
    ESP_LOGI(TAG, "Turning on the player... %s\n", LOG_CHECK((fsm_state_get(&music_player) == ST_PAUSED)));
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    fsm_dispatch(&music_player, EV_PLAY, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> PLAYING -> NORMAL state
    ESP_LOGI(TAG, "Starting playback... %s\n", LOG_CHECK((fsm_state_get(&music_player) == ST_NORMAL)));
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    fsm_dispatch(&music_player, EV_MODE_CHANGE, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> PLAYING -> SHUFFLE state
    ESP_LOGI(TAG, "Changing play mode... %s\n", LOG_CHECK((fsm_state_get(&music_player) == ST_SHUFFLE)));
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    fsm_dispatch(&music_player, EV_MENU, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> MENU state
    ESP_LOGI(TAG, "Opening menu... %s\n", LOG_CHECK((fsm_state_get(&music_player) == ST_MENU)));
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    fsm_dispatch(&music_player, EV_VOLUME_UP, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> MENU -> VOLUME_ADJUST state
    ESP_LOGI(TAG, "Adjusting volume... %s\n", LOG_CHECK((fsm_state_get(&music_player) == ST_VOLUME_ADJUST)));
    vTaskDelay(10 / portTICK_PERIOD_MS);

    fsm_dispatch(&music_player, EV_BACK, NULL);
    ret |= fsm_run(&music_player);  // Should be back in ON -> MENU state
    ESP_LOGI(TAG, "Going back to menu... %s\n", LOG_CHECK((fsm_state_get(&music_player) == ST_MENU)));
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    fsm_dispatch(&music_player, EV_SELECT, NULL);
    ret |= fsm_run(&music_player);  // Should be in ON -> MENU -> PLAYLIST_SELECT state
    ESP_LOGI(TAG, "Selecting playlist... %s\n", LOG_CHECK((fsm_state_get(&music_player) == ST_PLAYLIST_SELECT)));
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    fsm_dispatch(&music_player, EV_LOW_BATTERY, NULL);
    ret |= fsm_run(&music_player);  // Should transition to LOW_BATTERY state
    ESP_LOGI(TAG, "Low battery event... %s\n", LOG_CHECK((fsm_state_get(&music_player) == ST_LOW_BATTERY)));
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    fsm_dispatch(&music_player, EV_CHARGE, NULL);
    ret |= fsm_run(&music_player);  // Should transition back to ON state
    ESP_LOGI(TAG, "Charging the player... %s\n", LOG_CHECK((fsm_state_get(&music_player) == ST_PAUSED)));
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    fsm_dispatch(&music_player, EV_POWER, NULL);
    ret |= fsm_run(&music_player);  // Should be in OFF state
    ESP_LOGI(TAG, "Turning off the player... %s\n", LOG_CHECK((fsm_state_get(&music_player) == ST_OFF)));
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "--- End of Complex Music Player Simulation %s---\n", LOG_CHECK((ret == 0)));

    return ret;
}