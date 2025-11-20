#include "messenger.h"
#include "tkjhat/sdk.h"
#include "credentials.h"
#include "pico/cyw43_arch.h"

#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>

#ifndef WIFI_SSID
#define WIFI_SSID "panoulu"
#define WIFI_PASSWORD NULL
#define AUTH CYW43_AUTH_OPEN
#endif

#define TARGET_IP "192.168.1.106"
#define UDP_PORT 12345
#define TX_BUFFER_SIZE 128

static void loopback_task(void *arg)
{
    char message[TX_BUFFER_SIZE];
    int count = 0;
    while (1)
    {
        receive_udp_message(message, sizeof(message));
        printf("Received: %s\n", message);
        snprintf(message, sizeof(message), "Hello %d", count++);
        send_udp_message(message);
        printf("Sent: %s\n", message);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

int main(void)
{
    sleep_ms(6000); // Wait for serial to initialize
    stdio_init_all();
    setup_wifi(WIFI_SSID, WIFI_PASSWORD, AUTH, 30000, TARGET_IP, UDP_PORT);
    start_udp_receiver(UDP_PORT);
    udp_tasks_create();
    xTaskCreate(loopback_task, "loopback", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
    toggle_info_task(true);

    // Start the scheduler
    vTaskStartScheduler();

    // Should never reach here
    return 0;
}
