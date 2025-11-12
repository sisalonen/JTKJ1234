
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <hardware/watchdog.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <math.h>
#include <stdlib.h>

#include "tkjhat/sdk.h"

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048

// Button related defines
#define DEBOUNCE_MS 100
#define LONG_PRESS_DURATION_MS 1000

// Display related defines
#define DISPLAY_LINE_LEN 200
#define DISPLAY_DYNAMIC_LINES 3

#define INPUT_BUFFER_SIZE 256

#define TIME_UNIT 400
#define DOT_DURATION TIME_UNIT
#define DASH_DURATION (3 * TIME_UNIT)
#define GAP_DURATION TIME_UNIT
#define LETTER_GAP_DURATION (3 * TIME_UNIT)
#define WORD_GAP_DURATION (7 * TIME_UNIT)

TaskHandle_t myMainTask = NULL;
QueueHandle_t pitchQueue = NULL;

char message_str[DISPLAY_LINE_LEN + 1] = {0};
char receive_msg_str[DISPLAY_LINE_LEN] = {0};
char display_msg[DISPLAY_LINE_LEN] = {0};

typedef enum
{
    IDLE,
    ANGLE,
    LUX
} SensorState_t;
typedef enum
{
    MENU,
    MSG_GEN
} ProgramState_t;

ProgramState_t programState = MENU;

/* -------------------------------------------------------------------------- */
/*                                   sensors                                  */
/* -------------------------------------------------------------------------- */
#define ALPHA 0.98f

float pitch = 0.0f;

TaskHandle_t mySensorTask = NULL;
TaskHandle_t hReceiveTask = NULL;
TaskHandle_t myBlinkTask = NULL;
SensorState_t sensorState = IDLE;

float update_orientation(float ax, float ay, float az)
{
    float pitch_acc = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    pitch = pitch_acc;
    // // printf("Pitch: %.2f degrees\n", pitch);
    xQueueSend(pitchQueue, &pitch, 0); // Send pitch to queue
    return pitch;
}

void update_angle()
{
    float ax, ay, az, gx, gy, gz, temp;
    float previous;
    ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &temp);
    pitch = update_orientation(ax, ay, az);
    // recovery action for i2c data corruption
    if (previous == pitch)
    {
        init_ICM42670();
        ICM42670_start_with_default_values();
        // printf("restarted ICM42670");
    }
    previous = pitch;
}

void interpret_lux()
{
    init_veml6030();
    sleep_ms(500);
    float lux_off_val = (float)veml6030_read_light();
    TickType_t onStart;
    bool previous;
    // float previous_lux = 0.0f;

    for (;;)

    {
        if (sensorState != LUX)
            return;

        float lux_val = (float)veml6030_read_light();
        // printf("lux off val:%.2f", lux_off_val);
        // printf("lux on val: %.2f", lux_val);
        bool isOn = lux_val > lux_off_val + 20.0f;

        if (isOn && !previous)
        {
            onStart = xTaskGetTickCount();
            previous = true;
            // printf("on detected\n");
        }
        else if (!isOn && previous)
        {

            TickType_t duration = xTaskGetTickCount() - onStart;
            bool dash = duration > pdMS_TO_TICKS(DASH_DURATION);
            // printf("expected dash dura: %ld\n", DASH_DURATION);
            // printf("dura: %ld\n", pdTICKS_TO_MS(duration));

            if (dash)
            {
                // printf("dash\n");
                strncat(message_str, "-", sizeof(message_str) - strlen(message_str) - 1);
            }
            else
            {
                // printf("dot\n");
                strncat(message_str, ".", sizeof(message_str) - strlen(message_str) - 1);
            }
            // printf("Current message: { %s }\n", message_str);
            previous = false;
        }
        // previous_lux = lux_val;
        vTaskDelay(pdMS_TO_TICKS(TIME_UNIT / 3));
    }
}

static void sensor_task(void *arg)
{
    (void)arg;
    for (;;)
    {

        switch (sensorState)
        {
        case IDLE:
            vTaskSuspend(NULL);
            // printf("sensor task resumed\n");
            break;
        case ANGLE:
            // printf("angle sensor start\n");
            update_angle();
            sensorState = IDLE;
            break;
        case LUX:
            // printf("lux sensor start\n");
            interpret_lux();
            break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                                   buttons                                  */
/* -------------------------------------------------------------------------- */

typedef enum
{
    B_NONE,
    B1_SHORT,
    B1_LONG,
    B2_SHORT,
    B2_LONG
} buttonEvent_t;
typedef struct
{
    uint gpio;
    TickType_t pressStart;
    TickType_t lastInterrupt;
    bool longPressDetected;
} ButtonState_t;

buttonEvent_t bEvent = B_NONE;

TaskHandle_t myButton1Task = NULL;
TaskHandle_t myButton2Task = NULL;

ButtonState_t button1 = {.gpio = BUTTON1, .pressStart = 0, .lastInterrupt = 0, .longPressDetected = false};
ButtonState_t button2 = {.gpio = BUTTON2, .pressStart = 0, .lastInterrupt = 0, .longPressDetected = false};

void button_handler(uint gpio, uint32_t events);

void toggle_button_irs(bool status)
{
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, status, button_handler);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, status);
}

void button_handler(uint gpio, uint32_t events)
{
    {

        TickType_t now = xTaskGetTickCountFromISR();

        // ternary operator to decide which button data we point to.
        ButtonState_t *btn = (gpio == BUTTON1) ? &button1 : &button2;
        TaskHandle_t taskHandle = (gpio == BUTTON1) ? myButton1Task : myButton2Task;

        // guard for unwanted interruptions that sometimes happen with single button press
        if (now - btn->lastInterrupt < pdMS_TO_TICKS(DEBOUNCE_MS))
        {
            return;
        }
        toggle_button_irs(false);

        btn->lastInterrupt = now;

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

void process_button(ButtonState_t *btn, buttonEvent_t shortPressEvent, buttonEvent_t longPressEvent)
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

        vTaskDelay(pdMS_TO_TICKS(200));
        toggle_button_irs(true);
    }
    vTaskResume(myMainTask);
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

/* -------------------------------------------------------------------------- */
/*                                   display                                  */
/* -------------------------------------------------------------------------- */

TaskHandle_t myDisplayTask = NULL;

typedef struct
{
    char topHeader[DISPLAY_LINE_LEN];
    char dynamicContent[DISPLAY_DYNAMIC_LINES][DISPLAY_LINE_LEN];
    char buttonInfo[DISPLAY_LINE_LEN];
} displayData_t;

displayData_t menu = {.topHeader = "-------MENU-------", .buttonInfo = "switch/-  select/boot"};
displayData_t angle_msg_menu = {.topHeader = "Angle message:", .buttonInfo = "gen/send  break/back"};
displayData_t lux_msg_menu = {.topHeader = "Lux message:", .buttonInfo = "start/send  flush/back"};
displayData_t msg_only = {.topHeader = "", .buttonInfo = ""};

displayData_t *displayPtr = NULL;

static void display_task(void *arg)
{
    (void)arg;

    for (;;)
    {
        vTaskSuspend(NULL);

        clear_display();
        write_text_xy(0, 2, displayPtr->topHeader);

        for (int i = 0; i < DISPLAY_DYNAMIC_LINES; i++)
        {
            if (displayPtr->dynamicContent[i][0] == '\0')
            {
                write_text_xy(0, (60 / 4) * (i + 1), "");
            }
            write_text_xy(0, (60 / 4) * (i + 1), displayPtr->dynamicContent[i]);
            memset(displayPtr->dynamicContent[i], 0, sizeof(displayPtr->dynamicContent[i]));
        }

        write_text_xy(0, 56, displayPtr->buttonInfo);
    }
}
/* -------------------------------------------------------------------------- */
/*                                main state machine                          */
/* -------------------------------------------------------------------------- */

void popup_print(const char *message, uint32_t duration)
{
    strcpy(msg_only.dynamicContent[1], message);
    displayData_t *tempPtr = displayPtr;
    displayPtr = &msg_only;
    vTaskResume(myDisplayTask);
    sleep_ms(duration);
    displayPtr = tempPtr;
    vTaskResume(myDisplayTask);
}

void msg_print(const char *message, bool msg_only)
{
    // // printf("\033[2J\033[H");
    if (message)
    {
        if (msg_only)
        {
            // printf("%s\n", message);
        }
        else
        {
            // printf("Current message: { %s }\n", message);
        }
    }

    strcpy(displayPtr->dynamicContent[0], message);
    strcpy(displayPtr->dynamicContent[1], "received msg:");
    strcpy(displayPtr->dynamicContent[2], receive_msg_str);

    vTaskResume(myDisplayTask);
}

void print_menu(bool mode)
{
    displayPtr = &menu;
    strcpy(displayPtr->dynamicContent[0], mode ? "* Angle" : "  Angle");
    strcpy(displayPtr->dynamicContent[1], mode ? "  Lux" : "* Lux");
    vTaskResume(myDisplayTask);
}
void msg_send(const char *message)
{
    if (!message)
        return;

    for (size_t i = 0; message[i] != '\0'; i++)
    {
        if (message[i] == '.' || message[i] == '-')
        {
            putchar(message[i]);
        }
        else if (message[i] == ' ')
        {
            putchar(' '); // preserve letter spacing if needed
        }
    }

    // End marker for Python decoder
    printf("  \n");
    fflush(stdout);
    // for (size_t i = 0; i < strlen(message); i++)
    //     // printf("%02X ", (unsigned char)message[i]);
    // // printf("\n");
}

void angle_gen_ctrl()
{
    displayPtr = &angle_msg_menu;
    switch (bEvent)
    {
    case B2_SHORT:
        sensorState = ANGLE;
        vTaskResume(mySensorTask);
        float current_pitch;
        if (xQueueReceive(pitchQueue, &current_pitch, pdMS_TO_TICKS(300)) == pdTRUE)
        {
            if (fabsf(current_pitch) > 45.0f)
            {
                strncat(message_str, ".", sizeof(message_str) - strlen(message_str) - 1);
            }
            else
            {
                strncat(message_str, "-", sizeof(message_str) - strlen(message_str) - 1);
            }
        }
        break;
    case B1_SHORT:
        strncat(message_str, " ", sizeof(message_str) - strlen(message_str) - 1);
        break;

    case B2_LONG:
        msg_send(message_str);
        memset(message_str, 0, sizeof(message_str));
        popup_print("Message sent!", 2000);
        break;

    case B1_LONG:
        memset(message_str, 0, sizeof(message_str));
        sensorState = IDLE;
        programState = MENU;
        break;
    }
}

bool lux_toggle = false;
void lux_gen_ctrl()
{
    displayPtr = &lux_msg_menu;
    switch (bEvent)
    {
    case B2_SHORT:
        // start lux recording
        lux_msg_menu.buttonInfo[0] = '\n';
        if (lux_toggle)
        {
            // printf("switch sensor state to idle");
            strcpy(lux_msg_menu.buttonInfo, "start/send flush/back");
            sensorState = IDLE;
        }
        else
        {
            // printf("switch sensor state to lux");
            strcpy(lux_msg_menu.buttonInfo, "stop/send  flush/back");
            sensorState = LUX;
            vTaskResume(mySensorTask);
        }
        lux_toggle = !lux_toggle;
        break;

    case B1_SHORT:
        memset(message_str, 0, sizeof(message_str)); // flush message
        break;

    case B2_LONG:
        msg_send(message_str);
        memset(message_str, 0, sizeof(message_str)); // flush message
        popup_print("Message sent!", 2000);
        break;

    case B1_LONG:
        memset(message_str, 0, sizeof(message_str));
        strcpy(lux_msg_menu.buttonInfo, "start/send flush/back");
        sensorState = IDLE;
        programState = MENU;
        lux_toggle = false;
        break;
    }
}

void msg_gen(bool genUsingAngle)
{
    if (genUsingAngle)
    {
        angle_gen_ctrl();
    }
    else
    {
        lux_gen_ctrl();
    }
    bEvent = B_NONE;
    msg_print(message_str, false);
}

static void main_task(void *arg)
{
    (void)arg;
    // vTaskDelay(pdMS_TO_TICKS(1000));
    bool angle = true;
    for (;;)
    {

        switch (programState)
        {
        case MENU:
            switch (bEvent)
            {
            case B2_SHORT:
                angle = !angle;
                break;

            case B1_SHORT:
                programState = MSG_GEN;

                bEvent = B_NONE;
                continue;
            case B1_LONG:
                watchdog_reboot(0, 0, 0);
            }
            print_menu(angle);
            break;

        case MSG_GEN:
            msg_gen(angle);
            if (programState == MENU)
            {
                continue;
            }
            break;
        }
        bEvent = B_NONE;
        vTaskSuspend(NULL);
    }
}

/* -------------------------------------------------------------------------- */
/*                                    uart                                    */
/* -------------------------------------------------------------------------- */

void red_led_on(uint32_t duration)
{
    toggle_red_led();
    sleep_ms(duration);
    toggle_red_led();
}

void blink_msg(const char *message)
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

static void receive_task(void *arg)
{
    (void)arg;
    char line[30];
    size_t index = 0;

    while (1)
    {
        // OPTION 1
        //  Using getchar_timeout_us https://www.raspberrypi.com/documentation/pico-sdk/runtime.html#group_pico_stdio_1ga5d24f1a711eba3e0084b6310f6478c1a
        //  take one char per time and store it in line array, until reeceived the \n
        //  The application should instead play a sound, or blink a LED.
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT)
        { // I have received a character
            if (c == '\r')
                continue; // ignore CR, wait for LF if (ch == '\n') { line[len] = '\0';
            if (c == '\n')
            {
                // terminate and process the collected line
                line[index] = '\0';
                // printf("__[RX]:\"%s\"__\n", line);
                strcpy(receive_msg_str, line);
                strcpy(msg_only.dynamicContent[0], "New message received:");
                vTaskResume(myBlinkTask);
                popup_print(receive_msg_str, 3000);
                // Print as debug in the output
                index = 0;
                vTaskDelay(pdMS_TO_TICKS(100)); // Wait for new message
            }
            else if (index < INPUT_BUFFER_SIZE - 1)
            {
                line[index++] = (char)c;
            }
            // else
            // { // Overflow: print and restart the buffer with the new character.
            //     line[INPUT_BUFFER_SIZE - 1] = '\0';
            //     // printf("__[RX]:\"%s\"__\n", line);
            //     index = 0;
            //     line[index++] = (char)c;
            // }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100)); // Wait for new message
        }
        // OPTION 2. Use the whole buffer.
        /*absolute_time_t next = delayed_by_us(get_absolute_time,500);//Wait 500 us
        int read = stdio_get_until(line,INPUT_BUFFER_SIZE,next);
        if (read == PICO_ERROR_TIMEOUT){
            vTaskDelay(pdMS_TO_TICKS(100)); // Wait for new message
        }
        else {
            line[read] = '\0'; //Last character is 0
            // printf("__[RX] \"%s\"\n__", line);
            vTaskDelay(pdMS_TO_TICKS(50));
        }*/
    }
}

/* -------------------------------------------------------------------------- */
/*                             templates and init                             */
/* -------------------------------------------------------------------------- */
TaskHandle_t myExampleTask = NULL;

static void example_task(void *arg)
{
    (void)arg;

    for (;;)
    {
        tight_loop_contents(); // Modify with application code here.
        // // printf(".- .- ... ..  --- -.  \n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

int main()
{
    // inits
    stdio_init_all();
    init_hat_sdk();

    init_button1();
    init_button2();
    gpio_pull_up(BUTTON1);
    gpio_pull_up(BUTTON2);
    // vTaskDelay(pdMS_TO_TICKS(6000));

    init_red_led();
    init_i2c_default();
    init_display();
    init_veml6030();
    init_ICM42670();
    ICM42670_start_with_default_values();

    pitchQueue = xQueueCreate(1, sizeof(float));

    // Task creation

    xTaskCreate(example_task,       // (en) Task function
                "example",          // (en) Name of the task
                DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,               // (en) Arguments of the task
                1,                  // (en) Priority of this task
                &myExampleTask);    // (en) A handle to control the execution of this task

    xTaskCreate(sensor_task,
                "sensor",
                DEFAULT_STACK_SIZE,
                NULL,
                3,
                &mySensorTask);

    xTaskCreate(button1_task,
                "button1",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &myButton1Task);

    xTaskCreate(button2_task,
                "button2",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &myButton2Task);

    xTaskCreate(display_task,
                "display",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &myDisplayTask);

    xTaskCreate(receive_task,
                "receive",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &hReceiveTask);

    xTaskCreate(blink_task,
                "blink",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &myBlinkTask);

    xTaskCreate(main_task,
                "main",
                DEFAULT_STACK_SIZE,
                NULL,
                2,
                &myMainTask);

    // Interruption callbacks
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, button_handler);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}
