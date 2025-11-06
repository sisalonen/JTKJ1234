
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
TaskHandle_t myMainTask = NULL;
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

void update_orientation(float ax, float ay, float az,
                        float gx, float gy, float gz,
                        float dt)
{
    // Compute accelerometer angles
    float pitch_acc = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;

    // Integrate gyroscope
    float pitch_gyro = pitch + gy * dt;

    // Complementary filter
    pitch = ALPHA * pitch_gyro + (1.0f - ALPHA) * pitch_acc;
}

static void sensor_task(void *arg)
{
    (void)arg;

    float ax, ay, az, gx, gy, gz, temp;
    unsigned long last_time = time_us_32();

    for (;;)
    {
        switch (sensorState)
        {
        case IDLE:
            // printf("Suspending sensor\n");
            // vTaskSuspend(NULL);
            // printf("Resuming sensor\n");
            vTaskDelay(pdMS_TO_TICKS(50));

        case ANGLE:

            ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp);

            unsigned long now = time_us_32();
            float dt = (now - last_time) / 1e6f;
            last_time = now;

            update_orientation(ax, ay, az, gx, gy, gz, dt);

            // printf("\033[2J\033[H");
            // printf("%.2f", pitch);
            vTaskDelay(pdMS_TO_TICKS(3));
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                                   buttons                                  */
/* -------------------------------------------------------------------------- */

typedef enum
{
    OFF,
    B1_FALL,
    B1_RISE,
    B2_FALL,
    B2_RISE,
    B1_SHORT,
    B1_LONG,
    B2_SHORT,
    B2_LONG
} buttonEvent_t;

buttonEvent_t bEvent = OFF;

TickType_t lastInterrupt1, pressStart1, pressStop1 = 0;
TickType_t lastInterrupt2, pressStart2, pressStop2 = 0;

TaskHandle_t myButtonTask = NULL;

static void button_task(void *arg)
{
    (void)arg;

    for (;;)
    {
        TickType_t duration;
        vTaskSuspend(NULL);
        if (bEvent == B1_FALL)
        {
            duration = pressStop1 - pressStart1;
            if (duration < pdMS_TO_TICKS(1000))
            {
                bEvent = B1_SHORT;
            }
            else
            {
                bEvent = B1_LONG;
            }
        }
        else if (bEvent == B2_FALL)
        {

            duration = pressStop2 - pressStart2;
            if (duration < pdMS_TO_TICKS(1000))
            {
                bEvent = B2_SHORT;
            }
            else
            {
                bEvent = B2_LONG;
            }
        }
        vTaskResume(myMainTask);
    }
}

void button_handler(uint gpio, uint32_t events)
{
    {

        TickType_t now = xTaskGetTickCountFromISR();

        if (gpio == BUTTON1)
        {
            if (now - lastInterrupt1 < pdMS_TO_TICKS(100))
                return;
            lastInterrupt1 = now;

            if (events & GPIO_IRQ_EDGE_RISE)
            {

                pressStart1 = now;
                bEvent = B1_RISE;
            }
            else if (events & GPIO_IRQ_EDGE_FALL)
            {
                pressStop1 = now;
                bEvent = B1_FALL;

                xTaskResumeFromISR(myButtonTask);
            }
        }
        if (gpio == BUTTON2)
        {
            if (now - lastInterrupt2 < pdMS_TO_TICKS(100))
                return;
            lastInterrupt2 = now;

            if (events & GPIO_IRQ_EDGE_RISE)
            {

                pressStart2 = now;
                bEvent = B2_RISE;
            }
            else if (events & GPIO_IRQ_EDGE_FALL)
            {
                pressStop2 = now;
                bEvent = B2_FALL;

                xTaskResumeFromISR(myButtonTask);
            }
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
        vTaskSuspend(mySensorTask);
        init_display();
        clear_display();
        write_text_xy(4, 24, display_msg);
        display_msg[0] = '\0'; // flush message
        vTaskResume(mySensorTask);
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
    printf("\033[2J\033[H");
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
        printf("f-pitch %.2f", pitch);
        printf("abs_pitch %d", pitch);
        if (fabsf(pitch) > 45.0f)
        {
            strcat(message_str, ".");
        }
        else
        {
            strcat(message_str, "-");
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
            bEvent = OFF;
        case MSG_GEN:
            msg_gen();
            break;
        }
        bEvent = OFF;
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
    init_ICM42670();
    // clear_display();
    // write_text("message");

    sleep_ms(3000);
    ICM42670_start_with_default_values();

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

    xTaskCreate(button_task,
                "button",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &myButtonTask);

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
