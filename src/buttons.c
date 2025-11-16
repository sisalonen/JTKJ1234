#include "common.h"
#include "buttons.h"
#include "controller.h"
#include "tkjhat/sdk.h"

#define DEBOUNCE_MS 100
#define LONG_PRESS_DURATION_MS 1000

typedef struct
{
    uint gpio;
    TickType_t pressStart;
    TickType_t lastInterrupt;
    bool longPressDetected;
} ButtonState_t;

buttonEvent_t bEvent = B_NONE;

static TaskHandle_t myButton1Task = NULL;
static TaskHandle_t myButton2Task = NULL;
static ButtonState_t button1 = {.gpio = BUTTON1, .pressStart = 0, .lastInterrupt = 0, .longPressDetected = false};
static ButtonState_t button2 = {.gpio = BUTTON2, .pressStart = 0, .lastInterrupt = 0, .longPressDetected = false};

static void button_handler(uint gpio, uint32_t events);
static void button1_task(void *arg);
static void button2_task(void *arg);
static void button_health_mgr(void *arg);

void button_task_create(void)
{
    xTaskCreate(button1_task, "Button1Task", DEFAULT_STACK_SIZE, NULL, 2, &myButton1Task);
    xTaskCreate(button2_task, "Button2Task", DEFAULT_STACK_SIZE, NULL, 2, &myButton2Task);
    xTaskCreate(button_health_mgr, "ButtonHealthMgr", DEFAULT_STACK_SIZE, NULL, 1, NULL);
}

void toggle_button_irs(bool status)
{
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, status, button_handler);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, status);
}

static void button_handler(uint gpio, uint32_t events)
{
    {

        TickType_t now = xTaskGetTickCountFromISR();

        // ternary operator to decide which button data we point to.
        ButtonState_t *btn = (gpio == BUTTON1) ? &button1 : &button2;
        TaskHandle_t taskHandle = (gpio == BUTTON1) ? myButton1Task : myButton2Task;

        // duoble guard for unwanted interruptions that sometimes happen with single button press
        if (now - btn->lastInterrupt < pdMS_TO_TICKS(DEBOUNCE_MS))
        {
            return;
        }
        btn->lastInterrupt = now;
        toggle_button_irs(false);

        if (events & GPIO_IRQ_EDGE_RISE) // Button pressed
        {
            btn->pressStart = now;
            btn->longPressDetected = false;

            xTaskResumeFromISR(taskHandle);
        }
        else if (events & GPIO_IRQ_EDGE_FALL) // Button released
        {
            // After long press detection
            {
                if (btn->longPressDetected)
                    btn->longPressDetected = false;
                toggle_button_irs(true);
                return;
            }
            xTaskResumeFromISR(taskHandle);
        }
    }
}

static void process_button(ButtonState_t *btn, buttonEvent_t shortPressEvent, buttonEvent_t longPressEvent)
{
    // disable button interrutions for one cycle

    for (;;)
    {

        bool isPressed = gpio_get(btn->gpio) == 0;
        bool longPress = (xTaskGetTickCount() - btn->pressStart > pdMS_TO_TICKS(LONG_PRESS_DURATION_MS));

        if (!longPress && isPressed)
        {
            bEvent = shortPressEvent;
            break;
        }
        else if (longPress)
        {
            bEvent = longPressEvent;
            btn->longPressDetected = true;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
        toggle_button_irs(true);
    }
    vTaskResume(myControllerTask);
}

static void button1_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        vTaskSuspend(NULL);
        process_button(&button1, B1_SHORT, B1_LONG);
    }
}

static void button2_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        vTaskSuspend(NULL);
        process_button(&button2, B2_SHORT, B2_LONG);
    }
}

static void button_health_mgr(void *arg)
{
    (void)arg;
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(3000));
        // Re-enable interrupts in case they were missed
        toggle_button_irs(true);
    }
}
