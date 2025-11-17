#include "display.h"
#include "tkjhat/sdk.h"
#include "translator.h"
#include <string.h>
#include <stdio.h>

displayData_t menu = {.topHeader = "-------MENU-------", .buttonInfo = "switch/-  select/boot"};
displayData_t angle_msg_menu = {.topHeader = "Angle message:", .buttonInfo = "gen/send  break/back"};
displayData_t lux_msg_menu = {.topHeader = "Lux message:", .buttonInfo = "start/send  flush/back"};
displayData_t msg_only = {.topHeader = "", .buttonInfo = ""};
displayData_t *displayPtr = NULL;
TaskHandle_t myDisplayTask = NULL;

static void display_task(void *arg);

void popup_print(const char *message, uint32_t duration)
{
    strcpy(msg_only.dynamicContent[1], message);
    displayData_t *tempPtr = displayPtr;
    displayPtr = &msg_only;
    vTaskResume(myDisplayTask);
    sleep_ms(duration);
    displayPtr = tempPtr;
    vTaskResume(myDisplayTask);
}

void msg_print(char *message, bool translate)
{
    strcpy(displayPtr->dynamicContent[0], (translate) ? morseToText(message) : message);
    strcpy(displayPtr->dynamicContent[1], message);
    printf("translated: (%s)\n", displayPtr->dynamicContent[0]);
    printf("morse: (%s)\n", displayPtr->dynamicContent[1]);
    vTaskResume(myDisplayTask);
}

void print_menu(bool mode)
{
    displayPtr = &menu;
    strcpy(displayPtr->dynamicContent[0], mode ? "* Angle" : "  Angle");
    strcpy(displayPtr->dynamicContent[1], mode ? "  Lux" : "* Lux");
    vTaskResume(myDisplayTask);
}

void display_task_create(void)
{
    xTaskCreate(display_task, "DisplayTask", DEFAULT_STACK_SIZE, NULL, 2, &myDisplayTask);
}

static void display_task(void *arg)
{
    (void)arg;

    for (;;)
    {
        vTaskSuspend(NULL);
        if (xSemaphoreTake(I2C_semaphore, portMAX_DELAY) == pdTRUE)
        {
            // printf("display sema take");
            clear_display();
            write_text_xy(0, 2, displayPtr->topHeader);

            for (int i = 0; i < DISPLAY_DYNAMIC_LINES; i++)
            {
                if (displayPtr->dynamicContent[i][0] == '\0')
                {
                    write_text_xy(0, (60 / 4) * (i + 1), "");
                }
                write_text_xy(0, (60 / 4) * (i + 1), displayPtr->dynamicContent[i]);
                memset(displayPtr->dynamicContent[i], 0, sizeof(displayPtr->dynamicContent[i]));
            }

            write_text_xy(0, 56, displayPtr->buttonInfo);
            xSemaphoreGive(I2C_semaphore);
            // printf("display sema give");
        }
    }
}
