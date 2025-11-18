#include "common.h"
#include "tkjhat/sdk.h"

xSemaphoreHandle I2C_semaphore = NULL;
QueueHandle_t pitchQueue = NULL;

void red_led_on(uint32_t duration)
{
    toggle_red_led();
    sleep_ms(duration);
    toggle_red_led();
}