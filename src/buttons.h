#pragma once

typedef enum
{
    B_NONE,
    B1_SHORT,
    B1_LONG,
    B2_SHORT,
    B2_LONG
} buttonEvent_t;

extern buttonEvent_t bEvent;

void toggle_button_irs(bool status);
void button_task_create(void);
