
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <math.h>

#include "tkjhat/sdk.h"

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048

/* -------------------------------------------------------------------------- */
/*                                   sensors                                  */
/* -------------------------------------------------------------------------- */
#define ALPHA 0.98f
typedef enum
{
    IDLE,
    ANGLE,
} State_t;

float pitch = 0.0f;

TaskHandle_t mySensorTask = NULL;
State_t sensorState = IDLE;

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
            printf("Suspending sensor\n");
            vTaskSuspend(NULL);
            printf("Resuming sensor\n");

        case ANGLE:

            ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp);

            unsigned long now = time_us_32();
            float dt = (now - last_time) / 1e6f;
            last_time = now;

            update_orientation(ax, ay, az, gx, gy, gz, dt);

            printf("Pitch: %.2f°, \n", pitch);
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                                   buttons                                  */
/* -------------------------------------------------------------------------- */

typedef enum
{
    B1_SHORT,
    B1_LONG,
    B2_SHORT,
    B2_LONG
} Event_t;

// buttons currently bound to whatever actions for testing.
void button_handler(uint gpio, uint32_t events)
{
    {
        static TickType_t lastInterrupt1 = 0;
        static TickType_t lastInterrupt2 = 0;
        TickType_t now = xTaskGetTickCountFromISR();

        if (gpio == BUTTON1)
        {
            if (now - lastInterrupt1 < pdMS_TO_TICKS(50))
                return;
            lastInterrupt1 = now;

            if (sensorState == IDLE)
            {
                sensorState = ANGLE;
                xTaskResumeFromISR(mySensorTask);
            }
            else
            {
                sensorState = IDLE;
            }
        }

        else if (gpio == BUTTON2)
        {
            if (now - lastInterrupt2 < pdMS_TO_TICKS(50))
                return;
            lastInterrupt2 = now;

            toggle_red_led();
        }
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
        printf("Example task alive\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
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

    sleep_ms(3000);
    ICM42670_start_with_default_values();

    // Task creation

    BaseType_t result = xTaskCreate(example_task,       // (en) Task function
                                    "example",          // (en) Name of the task
                                    DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                                    NULL,               // (en) Arguments of the task
                                    1,                  // (en) Priority of this task
                                    &myExampleTask);    // (en) A handle to control the execution of this task

    result = xTaskCreate(sensor_task,        // (en) Task function
                         "sensor",           // (en) Name of the task
                         DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                         NULL,               // (en) Arguments of the task
                         2,                  // (en) Priority of this task
                         &mySensorTask);     // (en) A handle to control the execution of this task

    // Interruption callbacks
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL, true, button_handler);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_FALL, true);

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}
