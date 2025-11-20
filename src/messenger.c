#include "messenger.h"
#include "tkjhat/sdk.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <string.h>
#include <pico/stdlib.h>

#define TX_BUFFER_SIZE 128

static QueueHandle_t txQueue = NULL;
static QueueHandle_t rxQueue = NULL;
static ip_addr_t my_ip;
static bool info_task_on = false;
static char target_ip_str_global[32] = "255.255.255.255";
static int target_port_global = 12345;

TaskHandle_t mytxTask = NULL;

static void run_udp_unicast();
static void udp_tx(void *arg);
static void info_task(void *arg);

int setup_wifi(const char *ssid, const char *password, int auth, int timeout_ms,
               const char *target_ip_str, int target_port)
{

    // copy target ip/port into globals safely
    if (target_ip_str)
    {
        strncpy(target_ip_str_global, target_ip_str, sizeof(target_ip_str_global) - 1);
        target_ip_str_global[sizeof(target_ip_str_global) - 1] = '\0';
    }
    target_port_global = target_port;

    if (cyw43_arch_init())
    {
        printf("failed to initialise\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, auth, timeout_ms))
    {
        printf("failed to connect.\n");
        return 1;
    }
    else
    {
        printf("Connected.\n");
    }

    my_ip = cyw43_state.netif[CYW43_ITF_STA].ip_addr;
    printf("My IP is: %s\n", ipaddr_ntoa(&my_ip));

    txQueue = xQueueCreate(4, sizeof(char *)); // a small queue depth >1
    rxQueue = xQueueCreate(4, sizeof(char *));
    if (!txQueue || !rxQueue)
    {
        printf("Failed to create queues\n");
        return 1;
    }

    return 0;
}

void send_udp_message(const char *message)
{
    if (!txQueue)
        return;
    char *msg_copy = strdup(message);
    if (!msg_copy)
        return;
    // block a little if queue is full; you can use a timeout instead
    if (xQueueSend(txQueue, &msg_copy, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        // queue full: free msg_copy to avoid memory leak
        free(msg_copy);
    }
}

void receive_udp_message(char *buffer, size_t buffer_size)
{
    if (!rxQueue)
        return;
    char *received_msg = NULL;
    if (xQueueReceive(rxQueue, &received_msg, portMAX_DELAY) == pdTRUE && received_msg)
    {
        strncpy(buffer, received_msg, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        free(received_msg);
    }
}

void toggle_info_task(bool enable)
{
    info_task_on = enable;
}

void udp_tasks_create()
{
    xTaskCreate(udp_tx, "udpTxTask", 2048, NULL, 2, &mytxTask);
    xTaskCreate(info_task, "infoTask", 1024, NULL, 1, NULL);
}

static void simple_udp_receive_callback(void *arg, struct udp_pcb *upcb,
                                        struct pbuf *p, const ip_addr_t *addr, u16_t rx_port)
{
    if (p == NULL)
        return;

    // make a heap copy of the payload (respect p->len), null-terminate
    size_t len = p->len;
    char *copy = malloc(len + 1);
    if (!copy)
    {
        printf("malloc failed in rx callback\n");
        pbuf_free(p);
        return;
    }
    memcpy(copy, p->payload, len);
    copy[len] = '\0';

    printf("Received UDP packet from %s:%d: %s\n", ipaddr_ntoa(addr), rx_port, copy);

    // push the pointer to the queue (overwrite or send with timeout)
    if (xQueueSend(rxQueue, &copy, 0) != pdTRUE)
    {
        // queue full, discard oldest or free copy
        // try overwrite to keep latest:
        xQueueOverwrite(rxQueue, &copy);
    }

    pbuf_free(p); // safe: we copied payload
}
void start_udp_receiver(int port)
{
    // Wait until IP is assigned
    while (cyw43_state.netif[CYW43_ITF_STA].ip_addr.addr == 0)
    {
        sleep_ms(100);
    }

    cyw43_arch_lwip_begin();
    struct udp_pcb *pcb = udp_new();
    cyw43_arch_lwip_end();

    if (!pcb)
    {
        printf("PCB alloc failed\n");
        return;
    }

    cyw43_arch_lwip_begin();
    err_t err = udp_bind(pcb, IP_ADDR_ANY, port);
    cyw43_arch_lwip_end();

    printf("Bound UDP port %d, err=%d\n", port, err);

    cyw43_arch_lwip_begin();
    udp_recv(pcb, simple_udp_receive_callback, NULL);
    cyw43_arch_lwip_end();

    printf("Receiver ready!\n");
}

static void run_udp_unicast()
{
    ip_addr_t target_ip;
    if (!ipaddr_aton(target_ip_str_global, &target_ip))
    {
        printf("Invalid IP address: %s\n", target_ip_str_global);
        return;
    }

    cyw43_arch_lwip_begin();
    struct udp_pcb *pcb = udp_new();
    cyw43_arch_lwip_end();
    if (!pcb)
    {
        printf("UDP pcb alloc failed\n");
        return;
    }

    while (1)
    {
        char *queued_msg = NULL;
        if (xQueueReceive(txQueue, &queued_msg, portMAX_DELAY) != pdTRUE)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (!queued_msg)
        {
            // should not happen, but be defensive
            continue;
        }

        size_t msg_len = strnlen(queued_msg, TX_BUFFER_SIZE - 1);
        cyw43_arch_lwip_begin();
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, msg_len + 1, PBUF_RAM);
        cyw43_arch_lwip_end();

        if (!p)
        {
            printf("pbuf_alloc failed\n");
            free(queued_msg);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        memcpy(p->payload, queued_msg, msg_len);
        ((char *)p->payload)[msg_len] = '\0';
        free(queued_msg); // owned by us, free it now

        cyw43_arch_lwip_begin();
        err_t err = udp_sendto(pcb, p, &target_ip, target_port_global);
        pbuf_free(p);
        cyw43_arch_lwip_end();

        if (err == ERR_OK)
        {
            printf("Sent\n");
        }
        else
        {
            printf("udp_sendto err=%d\n", err);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void udp_tx(void *arg)
{
    (void)arg;
    run_udp_unicast();
}

static void info_task(void *arg)
{
    (void)arg;

    while (1)
    {
        if (info_task_on)
        {
            printf("My ip is: %s\n", ipaddr_ntoa(&my_ip));
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
