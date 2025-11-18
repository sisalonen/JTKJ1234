#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <semphr.h>

#define DEFAULT_STACK_SIZE 2048
#define DEFAULT_BUFFER_SIZE 256

// Sender expected to use time units of 250 ms.
// Receiving adjusted (200ms) to compensate for jitter during processing.
// Any faster interval is uncompatible with lux sensor.
#define TIME_UNIT 200
#define DOT_DURATION TIME_UNIT
#define DASH_DURATION (3 * TIME_UNIT)
#define GAP_DURATION TIME_UNIT
#define LETTER_GAP_DURATION (2 * TIME_UNIT)
#define WORD_GAP_DURATION (6 * TIME_UNIT)

extern xSemaphoreHandle I2C_semaphore;
extern QueueHandle_t pitchQueue;

void red_led_on(uint32_t duration);
