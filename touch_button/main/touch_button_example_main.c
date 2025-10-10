#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TIME_FOR_LONG_PRESS 500
#define BUTTON_NORMAL_STATE 1
#define BUTTON_PRESSED_STATE 0

// Define enumeration for buttons with GPIO numbers as values
typedef enum {
    Ctrl_C_Button = GPIO_NUM_6,  // GPIO 6
    Ctrl_V_Button = GPIO_NUM_7,  // GPIO 7
    BUTTON_0 = GPIO_NUM_0,  // GPIO 0
    BUTTON_COUNT = 3        // Total number of buttons (manually set to enum count)
} button_t;

// Array of button enum values for dynamic mask generation
static const button_t buttons[] = {Ctrl_C_Button, Ctrl_V_Button, BUTTON_0};

int buttonReg0Buffer [BUTTON_COUNT];
int buttonReg1Buffer [BUTTON_COUNT];
int buttonReg2Buffer [BUTTON_COUNT];
int buttonReg3Buffer [BUTTON_COUNT];

int buttonFlagBuffer [BUTTON_COUNT];

int buttonLongPressFlagBuffer [BUTTON_COUNT];
int timeOutForLongPress [BUTTON_COUNT];

void app_main(void) {

    GPIO_Init(); //Init input and output port

    button_Init(); //Init button flags

    // Create button debouncing task
    xTaskCreate(buttonDebouncing, "button_debouncing", 2048, buttons, 5, NULL);

    // Create button processing task
    xTaskCreate(buttonProcessing, "button_processing", 2048, buttons, 5, NULL);

    // Simple polling loop (print only on press)
    while (1) {

    }
}

void GPIO_Init ()
{
    esp_log_level_set("gpio", ESP_LOG_DEBUG);

    // Dynamically build pin_bit_mask for all buttons
    uint64_t pin_bit_mask = 0;
    for (int i = 0; i < BUTTON_COUNT; i++) {
        pin_bit_mask |= (1ULL << buttons[i]); // Use actual GPIO numbers from enum
    }

    // Configure GPIO directions and pull-ups for all buttons
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_bit_mask, // Dynamically generated mask
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    printf("Configured inputs with pull-ups");

    // Configure GPIO 48 as output
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_48); // Output
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    printf("Configured output");
}

void button_Init() 
{
    for(int i = 0 ; i < BUTTON_COUNT ; i++)
    {
        buttonReg0Buffer [i] = BUTTON_NORMAL_STATE;
        buttonReg1Buffer [i] = BUTTON_NORMAL_STATE;
        buttonReg2Buffer [i] = BUTTON_NORMAL_STATE;
        buttonReg3Buffer [i] = BUTTON_NORMAL_STATE;

        buttonFlagBuffer [i] = BUTTON_NORMAL_STATE;

        buttonLongPressFlagBuffer [i] = BUTTON_NORMAL_STATE;
        timeOutForLongPress [i] = TIME_FOR_LONG_PRESS;
    }
}

void buttonProcessing (void *pvParameters)
{
    bool any_button_pressed = false;
    for (int i = 0; i < BUTTON_COUNT; i++)
    {

        if (getButtonFlagAtIndex(buttons[i]) == BUTTON_PRESSED_STATE)
        { // Check if button is pressed (low)
            any_button_pressed = true;
            ESP_LOGI("Button %d pressed! (GPIO %d)", i, button);
        }

        if (getButtonLongPressFlagAtIndex(buttons[i]) == BUTTON_PRESSED_STATE)
        {
            any_button_pressed = true;
            ESP_LOGI("Button %d Long Pressed! (GPIO %d)", i, button);
        }

    }
    gpio_set_level(GPIO_NUM_48, any_button_pressed ? 1 : 0); // Turn on/off GPIO 48
    if (any_button_pressed) {
        ESP_LOGI("GPIO 48 ON");
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Small delay
}

int getButtonFlagAtIndex(button_t button)
{
    // Find the index of the button in the buttons array
    int index = -1;
    for (int i = 0; i < BUTTON_COUNT; i++) {
        if (buttons[i] == button) {
            index = i;
            break;
        }
    }

    // Return the flag if index is valid, otherwise return BUTTON_NORMAL_STATE
    if (index >= 0 && index < BUTTON_COUNT) {
        return buttonFlagBuffer[index];
    }
    return BUTTON_NORMAL_STATE; // Default or error case
}

int getButtonLongPressFlagAtIndex(button_t button)
{
    // Find the index of the button in the buttons array
    int index = -1;
    for (int i = 0; i < BUTTON_COUNT; i++) {
        if (buttons[i] == button) {
            index = i;
            break;
        }
    }

    // Return the long press flag if index is valid, otherwise return BUTTON_NORMAL_STATE
    if (index >= 0 && index < BUTTON_COUNT) {
        return buttonLongPressFlagBuffer[index];
    }
    return BUTTON_NORMAL_STATE; // Default or error case
}

void buttonDebouncing (void *pvParameters)
{
    // button_Init();

    button_t* buttons = (button_t *) pvParameters;

    while(1) 
    {
        for(int i = 0 ; i < BUTTON_COUNT && buttons[i] != NULL ; i++)
        {
            buttonReg0Buffer [i] = buttonReg1Buffer [i]; //30ms debouncing
            buttonReg1Buffer [i] = buttonReg2Buffer [i]; //20ms debouncing
            buttonReg2Buffer [i] = buttonReg3Buffer [i]; //10ms debouncing
            buttonReg3Buffer [i] = gpio_get_level(buttons[i]); //sampling

            if(buttonReg0Buffer[i] == BUTTON_PRESSED_STATE 
                && buttonReg1Buffer[i] == BUTTON_PRESSED_STATE
                && buttonReg2Buffer[i] == BUTTON_PRESSED_STATE)
            {
                buttonFlagBuffer [i] = BUTTON_PRESSED_STATE;
            }

            else
            {
                buttonFlagBuffer [i] = BUTTON_NORMAL_STATE;
            }

            if(buttonFlagBuffer [i] == BUTTON_PRESSED_STATE)
            {
                -- timeOutForLongPress [i];
            }

            if(timeOutForLongPress [i] <= 0)
            {
                buttonLongPressFlagBuffer [i] = BUTTON_PRESSED_STATE;
                timeOutForLongPress [i] = TIME_FOR_LONG_PRESS;
            }
        }

        
        vTaskDelay(100 / portTICK_PERIOD_MS); //10ms for each checking
    }
}