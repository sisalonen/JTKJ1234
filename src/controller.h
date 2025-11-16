#pragma once

#include "common.h"

typedef enum
{
    MENU,
    MSG_GEN
} ProgramState_t;

extern ProgramState_t programState;
extern TaskHandle_t myControllerTask;

void controller_task_create();
