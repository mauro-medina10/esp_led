/**
 * @file fsm.c
 * @author Mauro Medina (mauro.medina@kretz.com.ar)
 * @brief 
 * @version 1.0.1
 * @date 2024-07-17
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <stddef.h>
#include <stdbool.h>
#include "fsm.h"

#ifdef CONFIG_FREERTOS_PORT
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#else
#include "ring_buff.h"
#endif 

/* LOGGING MODULE REGISTER */
#ifdef RTT_CONSOLE
#include "SRC/RTT/rtt_log.h"
/// @brief Terminal RTT de logeo
#define FSM_RTT_TERMINAL 1	
/// @brief Nivel de Debug para logeo
#define FSM_RTT_LEVEL 2		

RTT_LOG_REGISTER(fsm, FSM_RTT_LEVEL, FSM_RTT_TERMINAL);
#endif
struct internal_ctx {
	int terminate:  1;
	int is_exit:    1;
    int handled:    1;
};


static void enter_state(fsm_t *fsm, const fsm_state_t *lca, const fsm_state_t *target, void *data) {
    fsm_state_t* state_path[MAX_HIERARCHY_DEPTH];
    fsm_state_t* state_target = (fsm_state_t*)target;
    int depth = 0;

    // Check for default substate
    while (state_target->default_substate) {
        state_target = state_target->default_substate;
    }

    // Build path from target to LCA (exclusive)
    for (fsm_state_t* s = (fsm_state_t*)state_target; s != lca && s != NULL; s = s->parent) {
        state_path[depth++] = s;
        if (depth >= MAX_HIERARCHY_DEPTH) break;
    }

    // Execute entry actions from LCA (exclusive) to target state
    for (int i = depth - 1; i >= 0; i--) {
        if (state_path[i]->entry_action) {
            state_path[i]->entry_action(fsm, data);
        }
    }

    // When source state is target state, execute entry action
    if((lca == state_target) && (depth == 0))
    {
        if(lca->entry_action) lca->entry_action(fsm, data);
    }

    fsm->current_state = (fsm_state_t*)state_target;
}

static void exit_state(fsm_t *fsm, const fsm_state_t *state, void *data) {
    for (fsm_state_t* s = fsm->current_state; s != state && s != NULL; s = s->parent) {
        if (s->exit_action) {
            s->exit_action(fsm, data);
        }
    }
}

static fsm_state_t* find_lca(fsm_state_t *s1, fsm_state_t *s2) {
    fsm_state_t *a = s1, *b = s2;
    while (a != b) {
        if (a == NULL) a = s2;
        else if (b == NULL) b = s1;
        else {
            a = a->parent;
            b = b->parent;
        }
    }
    return a;
}

int fsm_init(fsm_t *fsm, const fsm_transition_t *transitions, size_t num_transitions, const fsm_state_t* initial_state, void *initial_data) {
    struct internal_ctx *const internal = (void *)&fsm->internal;

    if(fsm == NULL || transitions == NULL || initial_state == NULL) return -1;
    if(num_transitions == 0) return -2;

    fsm->transitions         = transitions;
    fsm->num_transitions     = num_transitions;
    fsm->terminate_val       = 0;   
    internal->terminate      = false;
    internal->is_exit        = false;
    fsm->current_data        = initial_data;

#ifdef CONFIG_FREERTOS_PORT
    fsm->event_queue = xQueueCreate(FSM_MAX_EVENTS, sizeof(struct fsm_events_t));
    if(fsm->event_queue == NULL) return -3;
#else
    ringbuff_init(&fsm->event_queue, fsm->events_buff, FSM_MAX_EVENTS, sizeof(struct fsm_events_t));
#endif
#ifdef RTT_CONSOLE
    RTT_LOG("Init: %d\n", initial_state->state_id);
#endif
    enter_state(fsm, initial_state, initial_state, initial_data);

    return 0;
}

void fsm_dispatch(fsm_t *fsm, int event, void *data) {
    
    if(fsm == NULL) return;
    if(fsm->num_transitions == 0) return;

    struct fsm_events_t new_event = {event, data};

#ifdef CONFIG_FREERTOS_PORT
    if(xPortInIsrContext())
    {
        xQueueSendFromISR(fsm->event_queue, &new_event, NULL);
    }else
    {
        xQueueSend(fsm->event_queue, &new_event, 0);
    }
#else
    ringbuff_put(&fsm->event_queue, &new_event);
#endif    
}

static int fsm_process_events(fsm_t *fsm) {
    
    if(fsm == NULL) return -1;
    if(fsm->num_transitions == 0) return -2;

    struct internal_ctx *const internal = (void *)&fsm->internal;

    struct fsm_events_t current_event;

    // Ver si proceso todos los eventos o de a uno (actualmente procesa todos)
#ifdef CONFIG_FREERTOS_PORT
    int event_ready = 0;
    if(xPortInIsrContext())
    {
        event_ready = xQueueReceiveFromISR(fsm->event_queue, &current_event, NULL);
    }else
    {
        event_ready = xQueueReceive(fsm->event_queue, &current_event, 0);
    }
    while(event_ready) {
#else
    while (ringbuff_get(&fsm->event_queue, &current_event) == 0) {
#endif    
#ifdef RTT_CONSOLE        
        RTT_LOG("Event process: %d, state: %d\n", current_event.event, fsm->current_state->state_id);
#endif
        internal->handled = 0;

        fsm_state_t* current = fsm->current_state;
        while (internal->handled == 0 && current != NULL) 
        {
            for (size_t i = 0; i < fsm->num_transitions; ++i) {
                if (fsm->transitions[i].source_state == current && fsm->transitions[i].event == current_event.event) {
                    fsm_state_t* lca = find_lca(fsm->current_state, fsm->transitions[i].target_state);
#ifdef RTT_CONSOLE                        
                    RTT_LOG("Transition: %d to %d, lcs %d\n", current->state_id, fsm->transitions[i].target_state->state_id, lca->state_id);
#endif
                    exit_state(fsm, lca, current_event.data);
                    enter_state(fsm, lca, fsm->transitions[i].target_state, current_event.data);

                    /* No need to continue if terminate was set in the exit action */
                    if (internal->terminate) {
                        return fsm->terminate_val;
                    }
                    internal->handled = 1;
                }
            }
            current = current->parent;
        }
        
        if (internal->terminate) {
            return fsm->terminate_val;
        }
#ifdef CONFIG_FREERTOS_PORT        
        if(xPortInIsrContext())
        {
            event_ready = xQueueReceiveFromISR(fsm->event_queue, &current_event, NULL);
        }else
        {
            event_ready = xQueueReceive(fsm->event_queue, &current_event, 0);
        }
#endif        
    }
    return 0;
}

int fsm_run(fsm_t *fsm)
{
    if(fsm == NULL) return -1;

    struct internal_ctx *const internal = (void *)&fsm->internal;

    /* No need to continue if terminate was set */
	if (internal->terminate) {
		return fsm->terminate_val;
	}
    
    fsm_process_events(fsm);

    // Run state
    if (fsm->current_state->run_action) {
        fsm->current_state->run_action(fsm, fsm->current_data);
    }

    return 0;
}

int fsm_state_get(fsm_t *fsm)
{
    if(fsm == NULL) return FSM_ST_NONE;

    return fsm->current_state->state_id;
}

void fsm_terminate(fsm_t *fsm, int val)
{
    if(fsm == NULL) return;

    struct internal_ctx *const internal = (void *)&fsm->internal;
#ifdef RTT_CONSOLE
    RTT_LOG("FSM terminated\n");
#endif
    internal->terminate = true;
    fsm->terminate_val = val;  
}

int fsm_has_pending_events(fsm_t *fsm) {
    if(fsm == NULL) return -1;

#ifdef CONFIG_FREERTOS_PORT
        if(xPortInIsrContext())
        {
            return uxQueueMessagesWaitingFromISR(fsm->event_queue) > 0;
        }else
        {
            return uxQueueMessagesWaiting(fsm->event_queue) > 0;
        }
#else
    return ringbuff_num(&fsm->event_queue) > 0;
#endif
}

void fsm_flush_events(fsm_t *fsm) {
    
    if(fsm == NULL) return;

#ifdef CONFIG_FREERTOS_PORT
    xQueueReset(fsm->event_queue);
#else
    ringbuff_flush(&fsm->event_queue);
#endif
}