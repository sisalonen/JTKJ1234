#include "messenger.h"
#include "display.h"
#include "translator.h"
#include "sensors.h"
#include "tkjhat/sdk.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include <string.h>
#include <pico/stdlib.h>

#define UDP_PORT 12345
#define BEACON_MSG_LEN_MAX 127
#define BEACON_TARGET "100.67.23.230"
#define BEACON_INTERVAL_MS 1000
#define RECV_BUF_SIZE 128

#define WIFI_SSID "panoulu"
#define WIFI_PASSWORD NULL

TaskHandle_t hReceiveTask = NULL;
TaskHandle_t myBlinkTask = NULL;
TaskHandle_t myrxTask = NULL;
TaskHandle_t mytxTask = NULL;

char receive_msg_str[DEFAULT_BUFFER_SIZE + 1] = {0};

static void blink_msg(const char *message);
static void blink_task(void *arg);
static void receive_task(void *arg);
static void udp_rx(void *arg);
static void udp_tx(void *arg);

void messenger_task_create()
{
    xTaskCreate(receive_task, "ReceiveTask", DEFAULT_STACK_SIZE, NULL, 2, &hReceiveTask);
    xTaskCreate(blink_task, "BlinkTask", DEFAULT_STACK_SIZE, NULL, 2, &myBlinkTask);
    xTaskCreate(udp_rx, "udpRxTask", DEFAULT_STACK_SIZE, NULL, 2, &myrxTask);
    xTaskCreate(udp_tx, "udpTxTask", DEFAULT_STACK_SIZE, NULL, 2, &mytxTask);
}

int setup_wifi()
{
    if (cyw43_arch_init())
    {
        printf("failed to initialise\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_OPEN, 30000))
    {
        printf("failed to connect.\n");
        return 1;
    }
    else
    {
        printf("Connected.\n");
    }
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

void simple_udp_receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    printf("got here");
    if (p != NULL)
    {
        char *received_data = (char *)p->payload;
        printf("Received UDP packet from %s:%d: %s\n", ipaddr_ntoa(addr), port, received_data);
        pbuf_free(p); // Free the pbuf after processing the data
    }
}

void simple_udp_receiver()
{
    struct udp_pcb *pcb = udp_new();
    if (pcb == NULL)
    {
        printf("Failed to create UDP PCB\n");
        return;
    }

    ip_addr_t addr;
    IP4_ADDR(&addr, 0, 0, 0, 0); // Listen on all interfaces
    err_t err = udp_bind(pcb, &addr, UDP_PORT);
    if (err != ERR_OK)
    {
        printf("Failed to bind UDP PCB: %d\n", err);
        return;
    }

    udp_recv(pcb, simple_udp_receive_callback, NULL);

    printf("Simple UDP receiver listening on port %d...\n", UDP_PORT);

    // Add polling loop (could use cyw43_arch_poll() here if necessary)
    while (true)
    {
        // cyw43_arch_poll();
        sleep_ms(100);
    }
}
static void udp_rx(void *arg)
{
    (void)arg;
    simple_udp_receiver();
}

static void run_udp_beacon()
{
    struct udp_pcb *pcb = udp_new();

    ip_addr_t addr;
    ipaddr_aton(BEACON_TARGET, &addr);

    int counter = 0;
    while (true)
    {
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, BEACON_MSG_LEN_MAX + 1, PBUF_RAM);
        char *req = (char *)p->payload;
        memset(req, 0, BEACON_MSG_LEN_MAX + 1);
        snprintf(req, BEACON_MSG_LEN_MAX, "%d\n", counter);
        err_t er = udp_sendto(pcb, p, &addr, UDP_PORT);
        pbuf_free(p);
        if (er != ERR_OK)
        {
            printf("Failed to send UDP packet! error=%d", er);
        }
        else
        {
            printf("Sent packet %d\n", counter);
            counter++;
        }

        // Note in practice for this simple UDP transmitter,
        // the end result for both background and poll is the same

        vTaskDelay(pdMS_TO_TICKS(BEACON_INTERVAL_MS));
    }
}

static void udp_tx(void *arg)
{
    (void)arg;
    run_udp_beacon();
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
