#pragma once

#include "common.h"

#define DISPLAY_DYNAMIC_LINES 3

typedef struct
{
    char topHeader[DEFAULT_BUFFER_SIZE];
    char dynamicContent[DISPLAY_DYNAMIC_LINES][DEFAULT_BUFFER_SIZE];
    char buttonInfo[DEFAULT_BUFFER_SIZE];
} displayData_t;

extern displayData_t menu;
extern displayData_t angle_msg_menu;
extern displayData_t lux_msg_menu;
extern displayData_t msg_only;
extern displayData_t *displayPtr;

extern TaskHandle_t myDisplayTask;

void popup_print(const char *message, uint32_t duration);
void msg_print(char *message, bool translate);
void print_menu(bool mode);
void display_task_create(void);
