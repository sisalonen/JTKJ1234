#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int setup_wifi(const char *ssid, const char *password, int auth, int timeout_ms, const char *target_ip_str, int target_port);

void udp_tasks_create();

void start_udp_receiver(int port);

void send_udp_message(const char *message);

void receive_udp_message(char *buffer, size_t buffer_size);

void toggle_info_task(bool enable);