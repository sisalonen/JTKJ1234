#include "controller.h"
#include "display.h"
#include "sensors.h"
#include "buttons.h"
#include "messenger.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include <hardware/watchdog.h>

ProgramState_t programState = MENU;
TaskHandle_t myControllerTask = NULL;

static bool lux_toggle = false;

static void angle_gen_ctrl();
static void lux_gen_ctrl();
static void msg_gen(bool genUsingAngle);
static void controller_task(void *arg);

void controller_task_create()
{
    xTaskCreate(controller_task, "ControllerTask", DEFAULT_STACK_SIZE, NULL, 2, &myControllerTask);
}

static void angle_gen_ctrl()
{
    displayPtr = &angle_msg_menu;
    switch (bEvent)
    {
    case B2_SHORT:
        sensorState = ANGLE;
        vTaskResume(mySensorTask);
        float current_pitch;
        if (xQueueReceive(pitchQueue, &current_pitch, pdMS_TO_TICKS(300)) == pdTRUE)
        {
            if (fabsf(current_pitch) > 45.0f)
            {
                strncat(message_str, ".", sizeof(message_str) - strlen(message_str) - 1);
            }
            else
            {
                strncat(message_str, "-", sizeof(message_str) - strlen(message_str) - 1);
            }
        }
        break;
    case B1_SHORT:
        strncat(message_str, " ", sizeof(message_str) - strlen(message_str) - 1);
        break;

    case B2_LONG:
        printf("%s  \n", message_str);
        memset(message_str, 0, sizeof(message_str));
        popup_print("Message sent!", 2000);
        break;

    case B1_LONG:
        memset(message_str, 0, sizeof(message_str));
        sensorState = IDLE;
        programState = MENU;
        break;
    }
}

static void lux_gen_ctrl()
{
    displayPtr = &lux_msg_menu;
    switch (bEvent)
    {
    case B2_SHORT:
        // start lux recording
        lux_msg_menu.buttonInfo[0] = '\n';
        if (lux_toggle)
        {
            // printf("switch sensor state to idle");
            strcpy(lux_msg_menu.buttonInfo, "start/send flush/back");
            sensorState = IDLE;
        }
        else
        {
            // printf("switch sensor state to lux");
            strcpy(lux_msg_menu.buttonInfo, "stop/send  flush/back");
            sensorState = LUX;
            vTaskResume(mySensorTask);
        }
        lux_toggle = !lux_toggle;
        break;

    case B1_SHORT:
        memset(message_str, 0, sizeof(message_str)); // flush message
        break;

    case B2_LONG:
        printf("%s  \n", message_str);
        memset(message_str, 0, sizeof(message_str)); // flush message
        popup_print("Message sent!", 2000);
        break;

    case B1_LONG:
        memset(message_str, 0, sizeof(message_str));
        strcpy(lux_msg_menu.buttonInfo, "start/send flush/back");
        sensorState = IDLE;
        programState = MENU;
        lux_toggle = false;
        break;
    }
}

static void msg_gen(bool genUsingAngle)
{
    if (genUsingAngle)
    {
        angle_gen_ctrl();
    }
    else
    {
        lux_gen_ctrl();
    }
    bEvent = B_NONE;
    sleep_ms(100);
    msg_print(message_str, true);
}

static void controller_task(void *arg)
{
    (void)arg;
    // vTaskDelay(pdMS_TO_TICKS(1000));
    bool angle = true;

    for (;;)
    {

        switch (programState)
        {
        case MENU:
            switch (bEvent)
            {
            case B2_SHORT:
                angle = !angle;
                break;

            case B1_SHORT:
                programState = MSG_GEN;

                bEvent = B_NONE;
                continue;
            case B1_LONG:
                watchdog_reboot(0, 0, 0);
            }
            print_menu(angle);
            break;

        case MSG_GEN:
            msg_gen(angle);
            if (programState == MENU)
            {
                continue;
            }
            break;
        }
        bEvent = B_NONE;
        vTaskSuspend(NULL);
        // init_i2c_default();
    }
}