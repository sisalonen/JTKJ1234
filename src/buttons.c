#include "buttons.h"
#include "tkjhat/sdk.h"
#include <queue.h>
#include <task.h>

#define DEBOUNCE_MS 100
#define LONG_PRESS_DURATION_MS 1000

typedef struct
{
    uint gpio;
    TickType_t pressStart;
    TickType_t lastInterrupt;
    bool longPressDetected;
} ButtonState_t;

static QueueHandle_t buttonEventQueue;
static TaskHandle_t myButton1Task = NULL;
static TaskHandle_t myButton2Task = NULL;
static ButtonState_t button1 = {.gpio = BUTTON1, .pressStart = 0, .longPressDetected = false};
static ButtonState_t button2 = {.gpio = BUTTON2, .pressStart = 0, .longPressDetected = false};

static void button_handler(uint gpio, uint32_t events);
static void button1_task(void *arg);
static void button2_task(void *arg);

void button_get_event(ButtonEvent_t *event, TickType_t timeout)
{
    xQueueReceive(buttonEventQueue, event, timeout);
}

void button_task_create(void)
{
    buttonEventQueue = xQueueCreate(1, sizeof(ButtonEvent_t));
    xTaskCreate(button1_task, "Button1Task", 2048, NULL, 2, &myButton1Task);
    xTaskCreate(button2_task, "Button2Task", 2048, NULL, 2, &myButton2Task);
}

void toggle_button_irs(bool status)
{
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, status, button_handler);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, status);
}

/**
 * @brief Handler for both button 1 & 2 interrupts (Global IRQ events to be exact).
 * Checks which button pin caused the interrupt and updates corresponding ButtonState_t
 * for button tasks to process further.
 *
 * @param gpio which gpio pin caused the event (BUTTON1 or BUTTON2)
 * @param events event type (GPIO_IRQ_EDGE_FALL or GPIO_IRQ_EDGE_RISE)
 */
static void button_handler(uint gpio, uint32_t events)
{
    {

        TickType_t now = xTaskGetTickCountFromISR();

        // ternary operator to decide which button data we point to.
        ButtonState_t *btn = (gpio == BUTTON1) ? &button1 : &button2;
        TaskHandle_t taskHandle = (gpio == BUTTON1) ? myButton1Task : myButton2Task;

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
                return;
            }
            // Start corresponding button task
            xTaskResumeFromISR(taskHandle);
        }
    }
}

/**
 * @brief Polls button state (gpio_get) until button is either released (gpio_get == 1) or
 * has been held down for long enough (LONG_PRESS_DURATION_MS).
 *
 * @param btn Pointer to either button's data.
 * @param shortPressEvent button specific short press event name
 * @param longPressEvent button specific long press event name
 */
static void process_button(ButtonState_t *btn, ButtonEvent_t shortPressEvent, ButtonEvent_t longPressEvent)
{
    static ButtonEvent_t bEvent = B_NONE;
    for (;;)
    {

        // gpio_get(): https://deepbluembedded.com/raspberry-pi-pico-w-digital-inputs-outputs-c-sdk-rp2040/
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

        vTaskDelay(pdMS_TO_TICKS(200));
    }
    // Update the single element queue with most recent value for the getter
    xQueueOverwrite(buttonEventQueue, &bEvent);
}

/**
 * @brief Dedicated task for button1 status processing
 *
 */
static void button1_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        vTaskSuspend(NULL);
        process_button(&button1, B1_SHORT, B1_LONG);
    }
}

/**
 * @brief Dedicated task for button2 status processing
 *
 */
static void button2_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        vTaskSuspend(NULL);
        process_button(&button2, B2_SHORT, B2_LONG);
    }
}