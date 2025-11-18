#include "messenger.h"
#include "display.h"
#include "translator.h"
#include "sensors.h"
#include "tkjhat/sdk.h"

#include <string.h>
#include <pico/stdlib.h>

TaskHandle_t hReceiveTask = NULL;
TaskHandle_t myBlinkTask = NULL;

char receive_msg_str[DEFAULT_BUFFER_SIZE + 1] = {0};

static void blink_msg(const char *message);
static void blink_task(void *arg);
static void receive_task(void *arg);

void messenger_task_create()
{
    xTaskCreate(receive_task, "ReceiveTask", DEFAULT_STACK_SIZE, NULL, 2, &hReceiveTask);
    xTaskCreate(blink_task, "BlinkTask", DEFAULT_STACK_SIZE, NULL, 2, &myBlinkTask);
}

static void blink_msg(const char *message)
{
    size_t len = strlen(message);
    for (size_t i = 0; i < len; i++)
    {
        char c = message[i];
        if (c == '.')
        {
            // printf("blink dot");
            red_led_on(DOT_DURATION);
        }
        else if (c == '-')
        {
            // printf("blink dash");
            red_led_on(DASH_DURATION);
        }
        else if (c == ' ' || c == '/')
        {
            // printf("blink letter gap");
            sleep_ms(LETTER_GAP_DURATION);
            continue;
        }
        sleep_ms(GAP_DURATION);
    }
}

static void blink_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        vTaskSuspend(NULL);
        blink_msg(receive_msg_str);
    }
}

static void receive_task(void *arg)
{
    (void)arg;
    char line[30];
    size_t index = 0;

    while (1)
    {

        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT)
        {
            if (c == '\r')
                continue; // ignore CR, wait for LF if (ch == '\n') { line[len] = '\0';
            if (c == '\n')
            {
                // terminate and process the collected line
                line[index] = '\0';
                // printf("__[RX]:\"%s\"__\n", line);
                strcpy(receive_msg_str, line);
                strcpy(msg_only.dynamicContent[0], "New message received:");
                strcpy(msg_only.dynamicContent[2], morseToText(receive_msg_str));

                vTaskResume(myBlinkTask);
                popup_print(receive_msg_str, 3000);
                // Print as debug in the output
                index = 0;
                vTaskDelay(pdMS_TO_TICKS(100)); // Wait for new message
            }
            else if (index < DEFAULT_BUFFER_SIZE - 1)
            {
                line[index++] = (char)c;
            }
            // else
            // { // Overflow: print and restart the buffer with the new character.
            //     line[INPUT_BUFFER_SIZE - 1] = '\0';
            //     printf("__[RX]:\"%s\"__\n", line);
            //     index = 0;
            //     line[index++] = (char)c;
            // }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100)); // Wait for new message
        }
    }
}
