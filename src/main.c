
#include "common.h"
#include "messenger.h"
#include "sensors.h"
#include "buttons.h"
#include "display.h"
#include "controller.h"
#include "tkjhat/sdk.h"

int main(void)
{
    stdio_init_all();

    init_hat_sdk();

    init_button1();
    init_button2();
    gpio_pull_up(BUTTON1);
    gpio_pull_up(BUTTON2);

    init_red_led();
    init_i2c_default();
    init_display();
    init_veml6030();
    init_ICM42670();
    ICM42670_start_with_default_values();

    pitchQueue = xQueueCreate(1, sizeof(float));
    I2C_semaphore = xSemaphoreCreateBinary();

    if (I2C_semaphore != NULL)
    {
        xSemaphoreGive(I2C_semaphore);

        sensor_task_create();
        button_task_create();
        display_task_create();
        messenger_task_create();
        controller_task_create();

        toggle_button_irs(true);

        // Start the scheduler
        vTaskStartScheduler();
    }

    // Should never reach here

    return 0;
}
