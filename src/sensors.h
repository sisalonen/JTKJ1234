#pragma once

#include "common.h"

typedef enum
{
    IDLE,
    ANGLE,
    LUX
} SensorState_t;

extern float pitch;
extern TaskHandle_t mySensorTask;
extern SensorState_t sensorState;

void sensor_task_create(void);