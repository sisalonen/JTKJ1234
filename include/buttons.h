#pragma once

#include <stdbool.h>
#include <FreeRTOS.h>

typedef enum
{
    B_NONE,
    B1_SHORT,
    B1_LONG,
    B2_SHORT,
    B2_LONG
} ButtonEvent_t;

/**
 * @brief Suspend execution until new event is avaialable or timeout.
 * Assigns B_NONE if no event available before timeout.
 *
 * @param event Event instance to update
 * @param timeout How long to wait for new event.
 */
void button_get_event(ButtonEvent_t *event, TickType_t timeout);

/**
 * @brief enables or disables button irterruption handler callbacks
 *
 * @param status true -> enable buttons, false -> disable
 */
void toggle_button_irs(bool status);

/**
 * @brief Creates separate tasks for two individual buttons
 *
 */
void button_task_create(void);
