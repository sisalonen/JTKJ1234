
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <math.h>
#include <stdlib.h>

#include "tkjhat/sdk.h"

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048
#define DEBOUNCE_MS 50
#define LONG_PRESS_DURATION_MS 1500

TaskHandle_t myMainTask = NULL;
QueueHandle_t pitchQueue = NULL;

char message_str[80] = "";
char display_msg[80] = "";

/* -------------------------------------------------------------------------- */
/*                                   sensors                                  */
/* -------------------------------------------------------------------------- */
#define ALPHA 0.98f
typedef enum
{
    IDLE,
    ANGLE,
} SensorState_t;

float pitch = 0.0f;

TaskHandle_t mySensorTask = NULL;
SensorState_t sensorState = ANGLE;

void update_orientation(float ax, float ay, float az)
{
    float pitch_acc = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    pitch = pitch_acc;
    xQueueSend(pitchQueue, &pitch, 0); // Send pitch to queue
}

static void sensor_task(void *arg)
{
    (void)arg;

    float ax, ay, az, gx, gy, gz, temp;

    for (;;)
    {
        vTaskSuspend(NULL);
        ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp);

        update_orientation(ax, ay, az);

        printf("Pitch: %.2f degrees\n", pitch);
    }
}

/* -------------------------------------------------------------------------- */
/*                                   buttons                                  */
/* -------------------------------------------------------------------------- */

typedef enum
{
    B_NONE,
    B1_SHORT,
    B1_LONG,
    B2_SHORT,
    B2_LONG
} buttonEvent_t;
typedef struct
{
    uint gpio;
    TickType_t pressStart;
    TickType_t lastInterrupt;
    bool longPressDetected;
} ButtonState_t;

buttonEvent_t bEvent = B_NONE;

TaskHandle_t myButton1Task = NULL;
TaskHandle_t myButton2Task = NULL;

ButtonState_t button1 = {.gpio = BUTTON1, .pressStart = 0, .lastInterrupt = 0, .longPressDetected = false};
ButtonState_t button2 = {.gpio = BUTTON2, .pressStart = 0, .lastInterrupt = 0, .longPressDetected = false};

void process_button(ButtonState_t *btn, buttonEvent_t shortPressEvent, buttonEvent_t longPressEvent)
{
    for (;;)
    {
        bool isPressed = gpio_get(btn->gpio) == 0;
        bool longPress = (xTaskGetTickCount() - btn->pressStart > pdMS_TO_TICKS(LONG_PRESS_DURATION_MS));

        if (!longPress && isPressed)
        {
            bEvent = shortPressEvent;
            break;
        }
        else if (longPress)
        {
            bEvent = longPressEvent;
            btn->longPressDetected = true;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskResume(myMainTask);
}

static void button1_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        vTaskSuspend(NULL);
        process_button(&button1, B1_SHORT, B1_LONG);
    }
}

static void button2_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        vTaskSuspend(NULL);
        process_button(&button2, B2_SHORT, B2_LONG);
    }
}

void button_handler(uint gpio, uint32_t events)
{
    {

        TickType_t now = xTaskGetTickCountFromISR();

        // ternary operator to decide which button data we point to.
        ButtonState_t *btn = (gpio == BUTTON1) ? &button1 : &button2;
        TaskHandle_t taskHandle = (gpio == BUTTON1) ? myButton1Task : myButton2Task;

        // guard for unwanted interruptions that sometimes happen with single button press
        if (now - btn->lastInterrupt < pdMS_TO_TICKS(DEBOUNCE_MS))
        {
            return;
        }

        btn->lastInterrupt = now;

        if (events & GPIO_IRQ_EDGE_RISE) // Button pressed
        {
            btn->pressStart = now;
            btn->longPressDetected = false;

            xTaskResumeFromISR(taskHandle);
        }
        else if (events & GPIO_IRQ_EDGE_FALL) // Button released
        {
            // After long press detection
            {
                if (btn->longPressDetected)
                    btn->longPressDetected = false;
                return;
            }
            xTaskResumeFromISR(taskHandle);
        }
    }
}
/* -------------------------------------------------------------------------- */
/*                                   display                                  */
/* -------------------------------------------------------------------------- */

TaskHandle_t myDisplayTask = NULL;

static void display_task(void *arg)
{
    (void)arg;

    for (;;)
    {
        vTaskSuspend(NULL);
        clear_display();
        write_text_xy(4, 24, display_msg);
        display_msg[0] = '\0'; // flush message
    }
}
/* -------------------------------------------------------------------------- */
/*                                main state machine                          */
/* -------------------------------------------------------------------------- */
typedef enum
{
    MENU,
    MSG_GEN
} ProgramState_t;

ProgramState_t programState = MENU;

void msg_print(const char *message, bool msg_only)
{
    // printf("\033[2J\033[H");
    if (message)
    {
        if (msg_only)
        {
            printf("%s\n", message);
        }
        else
        {
            printf("Current message: { %s }\n", message);
        }
    }
    strcat(display_msg, message);
    vTaskResume(myDisplayTask);
}

void msg_gen()
{
    switch (bEvent)
    {
    case B1_SHORT:
        vTaskResume(mySensorTask);
        float current_pitch;
        if (xQueueReceive(pitchQueue, &current_pitch, pdMS_TO_TICKS(300)) == pdTRUE)
        {
            if (fabsf(current_pitch) > 45.0f)
            {
                strcat(message_str, ".");
            }
            else
            {
                strcat(message_str, "-");
            }
        }
        break;

    case B2_SHORT:
        strcat(message_str, "␣");
        break;

    case B1_LONG:
        msg_print("Message sent!", true);
        vTaskDelay(pdMS_TO_TICKS(4000));
        break;

    case B2_LONG:
        message_str[0] = '\0'; // flush message
        msg_print("Message flushed!", true);
        vTaskDelay(pdMS_TO_TICKS(4000));
        break;
    }
    msg_print(message_str, false);
}

static void main_task(void *arg)
{
    (void)arg;
    bool angle;
    for (;;)
    {
        vTaskSuspend(NULL);
        switch (programState)
        {
        case MENU:

            switch (bEvent)
            {
            case B1_SHORT:
                if (!angle)
                {
                    msg_print(" angle\n*lux", false);
                    angle = true;
                }
                else
                {
                    msg_print("*angle\n lux", false);
                    angle = false;
                }

                break;

            case B2_SHORT:
                programState = MSG_GEN;
                break;
            }
            break;

        case MSG_GEN:
            msg_gen();
            break;
        }
        bEvent = B_NONE;
    }
}

/* -------------------------------------------------------------------------- */
/*                             templates and init                             */
/* -------------------------------------------------------------------------- */
TaskHandle_t myExampleTask = NULL;

static void example_task(void *arg)
{
    (void)arg;

    for (;;)
    {
        tight_loop_contents(); // Modify with application code here.
        // printf("Example task alive\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

int main()
{
    // inits
    stdio_init_all();
    init_hat_sdk();

    init_button1();
    init_button2();
    gpio_pull_up(BUTTON1);
    gpio_pull_up(BUTTON2);

    init_red_led();
    init_i2c_default();
    init_display();
    init_ICM42670();
    // clear_display();
    // write_text("message");

    sleep_ms(3000);
    ICM42670_start_with_default_values();

    pitchQueue = xQueueCreate(1, sizeof(float));

    // Task creation

    xTaskCreate(example_task,       // (en) Task function
                "example",          // (en) Name of the task
                DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,               // (en) Arguments of the task
                1,                  // (en) Priority of this task
                &myExampleTask);    // (en) A handle to control the execution of this task

    xTaskCreate(main_task,
                "main",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &myMainTask);

    xTaskCreate(sensor_task,
                "sensor",
                DEFAULT_STACK_SIZE,
                NULL,
                3,
                &mySensorTask);

    xTaskCreate(button1_task,
                "button1",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &myButton1Task);

    xTaskCreate(button2_task,
                "button2",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &myButton2Task);

    xTaskCreate(display_task,
                "display",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &myDisplayTask);

    // Interruption callbacks
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, button_handler);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}
