#include "sensors.h"

#include "messenger.h"
#include "display.h"
#include "tkjhat/sdk.h"
#include <math.h>
#include <string.h>

float pitch = 0.0f;
TaskHandle_t mySensorTask = NULL;
SensorState_t sensorState = IDLE;

static float update_angle(float ax, float ay, float az);
static void process_angle();
static void interpret_lux();
static void sensor_task(void *arg);

void sensor_task_create(void)
{
    xTaskCreate(sensor_task, "SensorTask", DEFAULT_STACK_SIZE, NULL, 2, &mySensorTask);
}

static float update_angle(float ax, float ay, float az)
{
    // angle formula from:
    // https://forum.arduino.cc/t/getting-pitch-and-roll-from-acceleromter-data/
    float pitch_acc = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    pitch = pitch_acc;
    xQueueOverwrite(pitchQueue, &pitch);
    return pitch;
}

static void process_angle()
{
    float ax, ay, az, gx, gy, gz, temp;
    float previous;
    // init_ICM42670();
    if (xSemaphoreTake(I2C_semaphore, portMAX_DELAY) == pdTRUE)
    {
        ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp);
        pitch = update_angle(ax, ay, az);

        xSemaphoreGive(I2C_semaphore);
    }

    previous = pitch;
}

static void interpret_lux()
{
    init_veml6030();
    float lux_off_val;

    // Read the initial lux value to define the 'off' threshold.
    if (xSemaphoreTake(I2C_semaphore, portMAX_DELAY) == pdTRUE)
    {

        lux_off_val = (float)veml6030_read_light();
        xSemaphoreGive(I2C_semaphore);
    }

    TickType_t onStart;
    TickType_t lastTransitionTime = 0; // timestamp of when lux falls below isOn treshold
    bool previous = false;             // was the previous cycle on (true) or off (false)
    bool isOn = false;
    bool letterGapRegistered;

    for (;;)
    {
        if (sensorState != LUX)
            return;

        float lux_val;
        if (xSemaphoreTake(I2C_semaphore, portMAX_DELAY) == pdTRUE)
        {
            // printf("lux sema take");
            lux_val = (float)veml6030_read_light();
            xSemaphoreGive(I2C_semaphore);
            // printf("lux sema give");
        }

        isOn = lux_val > lux_off_val + 20.0f;

        if (isOn && !previous)
        {
            onStart = xTaskGetTickCount();
            previous = true;
            lastTransitionTime = xTaskGetTickCount();
        }
        else if (!isOn && previous)
        {

            TickType_t duration = xTaskGetTickCount() - onStart;
            bool dash = duration > pdMS_TO_TICKS(DASH_DURATION);

            if (dash)
            {
                // printf("dash\n");
                strncat(message_str, "-", sizeof(message_str) - strlen(message_str) - 1);
            }
            else
            {
                // printf("dot\n");
                strncat(message_str, ".", sizeof(message_str) - strlen(message_str) - 1);
            }
            msg_print(message_str, true);

            previous = false;
            lastTransitionTime = xTaskGetTickCount();
        }

        TickType_t currentTime = xTaskGetTickCount();

        if (!isOn && lastTransitionTime != 0)
        {
            TickType_t gapDuration = currentTime - lastTransitionTime;

            if (gapDuration >= pdMS_TO_TICKS(WORD_GAP_DURATION))
            {
                // printf("registered WORD gap\n");
                strncat(message_str, "  ", sizeof(message_str) - strlen(message_str) - 1);
                lastTransitionTime = 0;
            }
            else if (gapDuration >= pdMS_TO_TICKS(LETTER_GAP_DURATION) && !letterGapRegistered)
            {
                // printf("registered LETTER gap\n");
                strncat(message_str, " ", sizeof(message_str) - strlen(message_str) - 1);
                letterGapRegistered = true;
            }
        }
        else if (isOn)
        {
            // Reset state when light turns back on
            letterGapRegistered = false;
        }

        vTaskDelay(pdMS_TO_TICKS(TIME_UNIT / 3));
    }
}

static void sensor_task(void *arg)
{
    (void)arg;
    for (;;)
    {

        switch (sensorState)
        {
        case IDLE:
            vTaskSuspend(NULL);
            // printf("sensor task resumed\n");
            break;
        case ANGLE:
            // printf("angle sensor start\n");
            process_angle();
            sensorState = IDLE;
            break;
        case LUX:
            // printf("lux sensor start\n");
            interpret_lux();
            break;
        }
    }
}
