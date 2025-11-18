#include "controller.h"
#include "display.h"
#include "sensors.h"
#include "buttons.h"
#include "messenger.h"
#include "tkjhat/sdk.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/watchdog.h>

ProgramState_t programState = MENU;
TaskHandle_t myControllerTask = NULL;

static bool lux_toggle = false;

static void angle_gen_ctrl(ButtonEvent_t bEvent);
static void lux_gen_ctrl(ButtonEvent_t bEvent);
static void msg_gen(bool genUsingAngle, ButtonEvent_t bEvent);
static void controller_task(void *arg);

void controller_task_create()
{
    xTaskCreate(controller_task, "ControllerTask", DEFAULT_STACK_SIZE, NULL, 2, &myControllerTask);
}

static void angle_gen_ctrl(ButtonEvent_t bEvent)
{
    displayPtr = &angle_msg_menu;
    switch (bEvent)
    {
    case B2_SHORT:
        sensorState = ANGLE;
        vTaskResume(mySensorTask);
        float current_pitch;
        if (xQueueReceive(pitchQueue, &current_pitch, portMAX_DELAY) == pdTRUE)
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
        popup_print("Message sent!", 2000);
        break;

    case B1_LONG:
        memset(message_str, 0, sizeof(message_str));
        sensorState = IDLE;
        programState = MENU;
        break;
    }
}

static void lux_gen_ctrl(ButtonEvent_t bEvent)
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

static void msg_gen(bool genUsingAngle, ButtonEvent_t bEvent)
{
    if (genUsingAngle)
    {
        angle_gen_ctrl(bEvent);
    }
    else
    {
        lux_gen_ctrl(bEvent);
    }
    sleep_ms(100);
    msg_print(message_str, true);
}

static void controller_task(void *arg)
{
    (void)arg;
    bool muted = true;
    bool angle = true;
    bool ignore_next_event = false;
    ButtonEvent_t bEvent = B_NONE;
    uint32_t value = 0;
    // Run one cycle with defaults during startup and suspend at end of the loop
    // waiting for new button event
    for (;;)
    {

        if (ignore_next_event)
        {
            bEvent = B_NONE;
            ignore_next_event = false; // only ignore once
        }

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
            case B2_LONG:
                muted = !muted;
                break;
            case B1_LONG:
                watchdog_reboot(0, 0, 0);
            }
            print_menu(angle);
            break;

        case MSG_GEN:
            msg_gen(angle, bEvent);
            if (programState == MENU)
            {
                ignore_next_event = true; // button cooldown
                bEvent = B_NONE;
                continue;
            }
            break;
        }
        if (!muted)
        {
            buzzer_play_tone(1800, 30);
        }
        red_led_on(30);
        button_get_event(&bEvent, portMAX_DELAY); // suspend until new event available
    }
}