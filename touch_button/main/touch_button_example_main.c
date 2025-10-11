#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TICK 10
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
// int buttonReg4Buffer [BUTTON_COUNT];

int buttonFlagBuffer [BUTTON_COUNT];

int buttonLongPressFlagBuffer [BUTTON_COUNT];
int timeOutForLongPress [BUTTON_COUNT];

void GPIO_Init ();
void button_Init();
void buttonProcessing (void *pvParameters);
void buttonDebouncing (void *pvParameters);
void ID_Printout (void *pvParameters);
int getIndexOfButton(button_t button);
int getButtonFlagAtButton(button_t button);
void clearButtonFlagAtButton(button_t button);
int getButtonLongPressFlagAtButton(button_t button);
void clearButtonLongPressFlagAtButton(button_t button);
int getButtonFlagAtIndex(int index);
void clearButtonFlagAtIndex(int index);
int getButtonLongPressFlagAtIndex(int index);
void clearButtonLongPressFlagAtIndex(int index);

void app_main(void) {

    printf("GPIO Init\n");
    GPIO_Init(); //Init input and output port

    printf("Button Init\n");
    button_Init(); //Init button flags

    // Create button debouncing task
    printf("Button Debouncing task is Created\n");
    xTaskCreate(buttonDebouncing, "button_debouncing", 2048, (void *)buttons, 5, NULL);

    // Create button processing task
    printf("Button Processing task is Created\n");
    xTaskCreate(buttonProcessing, "button_processing", 2048, (void *)buttons, 5, NULL);

    printf("ID printout task is created\n");
    xTaskCreate(ID_Printout, "ID_printout", 1024, NULL, 5, NULL);

    // while (1)
    // {
    //     printf ("Hello World \n");

    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

    // Simple polling loop (print only on press)

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
    printf("Configured inputs with pull-ups\n");

    // Configure GPIO 48 as output
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_48); // Output
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    printf("Configured output\n");
}

void button_Init() 
{
    for(int i = 0 ; i < BUTTON_COUNT ; i++)
    {
        buttonReg0Buffer [i] = BUTTON_NORMAL_STATE;
        buttonReg1Buffer [i] = BUTTON_NORMAL_STATE;
        buttonReg2Buffer [i] = BUTTON_NORMAL_STATE;
        buttonReg3Buffer [i] = BUTTON_NORMAL_STATE;
        // buttonReg4Buffer [i] = BUTTON_NORMAL_STATE;

        buttonFlagBuffer [i] = BUTTON_NORMAL_STATE;

        buttonLongPressFlagBuffer [i] = BUTTON_NORMAL_STATE;
        timeOutForLongPress [i] = TIME_FOR_LONG_PRESS / TICK;
    }
}

void ID_Printout (void *pvParameters)
{
    while(1)
    {
        printf("My student ID is 2213278\n");

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void buttonDebouncing (void *pvParameters)
{
    // button_Init();

    button_t* buttons = (button_t *) pvParameters;

    while(1) 
    {
        for(int i = 0 ; i < BUTTON_COUNT ; i++)
        {
            buttonReg0Buffer [i] = buttonReg1Buffer [i]; //30ms debouncing
            buttonReg1Buffer [i] = buttonReg2Buffer [i]; //20ms debouncing
            buttonReg2Buffer [i] = gpio_get_level(buttons[i]); //sampling

            int temp = buttonReg3Buffer[i];

            if(buttonReg0Buffer [i] == buttonReg1Buffer [i] && buttonReg0Buffer [i] == buttonReg2Buffer [i])
            {
                temp = buttonReg2Buffer [i];
            }

            if(buttonReg3Buffer[i] != temp) //button switch from pressed into release or release into pressed
            {
                buttonReg3Buffer[i] = temp;
                timeOutForLongPress [i] = TIME_FOR_LONG_PRESS / TICK; //reset time out because we have switched into a new state
            }

            //if it is pressed and long pressed we will minus time out and update it as Long Pressed
            //if we use else command we will have a problem when NOT PRESSED state we also minus time out
            //and if it is pressed at time out <  origin Time For Long Press we will determine it as long pressed
            else if(buttonReg3Buffer[i] == BUTTON_PRESSED_STATE)
            {
                timeOutForLongPress [i] --;
            }

            //if it is pressed but TimeOutForKeyPressBuffer larger on or equal to origin Time For Long Press we will consider it as pressed one time
            if(buttonReg3Buffer [i] == BUTTON_PRESSED_STATE && timeOutForLongPress [i] >= TIME_FOR_LONG_PRESS / TICK)
            {
                buttonFlagBuffer[i] = BUTTON_PRESSED_STATE;
            }

            //if it is pressed and TimeOutForKeyPressBuffer equal to zero we will consider it as long pressed
            else if(buttonReg3Buffer[i] == BUTTON_PRESSED_STATE && timeOutForLongPress [i] <= 0)
            {
                buttonLongPressFlagBuffer[i] = BUTTON_PRESSED_STATE;
                timeOutForLongPress [i] = TIME_FOR_LONG_PRESS / TICK;
            }
        }

        
        vTaskDelay(TICK / portTICK_PERIOD_MS); //10ms for each checking
    }

    vTaskDelete ( NULL ) ;
}

void buttonProcessing (void *pvParameters)
{
    while (1)
    {
        bool any_button_pressed = false;
        for (int i = 0; i < BUTTON_COUNT; i++)
        {
            // int index = getIndexOfButton(buttons[i]);

            // if (getButtonFlagAtIndex(index) == BUTTON_PRESSED_STATE)
            if(getButtonFlagAtButton(buttons[i]) == BUTTON_PRESSED_STATE)
            { // Check if button is pressed (low)
                any_button_pressed = true;
                // clearButtonFlagAtIndex(index);
                clearButtonFlagAtButton(buttons[i]);
                printf("Button %d pressed! (GPIO %d) \n", i, buttons[i]);
                printf("ESP32 \n");
            }

            // if (getButtonLongPressFlagAtIndex(index) == BUTTON_PRESSED_STATE)
            if(getButtonLongPressFlagAtButton(buttons[i]) == BUTTON_PRESSED_STATE)
            {
                any_button_pressed = true;
                // clearButtonLongPressFlagAtIndex(index);
                clearButtonLongPressFlagAtButton(buttons[i]);
                printf("Button %d Long Pressed! (GPIO %d) \n", i, buttons[i]);
            }

        }
        gpio_set_level(GPIO_NUM_48, any_button_pressed ? 1 : 0); // Turn on/off GPIO 48
        // if (any_button_pressed) {
        //     printf("GPIO 48 ON \n");
        // }
        vTaskDelay(TICK / portTICK_PERIOD_MS); // Small delay
    }

    vTaskDelete ( NULL ) ;
}

int getIndexOfButton(button_t button)
{
    // Find the index of the button in the buttons array
    int index = -1;
    for (int i = 0; i < BUTTON_COUNT; i++) {
        if (buttons[i] == button) {
            index = i;
            return index;
        }
    }
    return -1;
}

int getButtonFlagAtButton(button_t button)
{
    // Find the index of the button in the buttons array
    int index = getIndexOfButton(button);

    // Return the flag if index is valid, otherwise return BUTTON_NORMAL_STATE
    if (index >= 0 && index < BUTTON_COUNT) {
        return buttonFlagBuffer[index];
    }
    return BUTTON_NORMAL_STATE; // Default or error case
}

void clearButtonFlagAtButton(button_t button)
{
    // Find the index of the button in the buttons array
    int index = getIndexOfButton(button);

    if (index >= 0 && index < BUTTON_COUNT) {
        buttonFlagBuffer [index] = BUTTON_NORMAL_STATE; //consider to add semaphore or mutex
    }
}

int getButtonLongPressFlagAtButton(button_t button)
{
    // Find the index of the button in the buttons array
    int index = getIndexOfButton(button);

    // Return the long press flag if index is valid, otherwise return BUTTON_NORMAL_STATE
    if (index >= 0 && index < BUTTON_COUNT) {
        return buttonLongPressFlagBuffer[index];
    }
    return BUTTON_NORMAL_STATE; // Default or error case
}

void clearButtonLongPressFlagAtButton(button_t button)
{
    // Find the index of the button in the buttons array
    int index = getIndexOfButton(button);

    if (index >= 0 && index < BUTTON_COUNT) {
        buttonLongPressFlagBuffer [index] = BUTTON_NORMAL_STATE; //consider to add semaphore or mutex
    }
}

int getButtonFlagAtIndex(int index)
{
    // Return the flag if index is valid, otherwise return BUTTON_NORMAL_STATE
    if (index >= 0 && index < BUTTON_COUNT) {
        return buttonFlagBuffer[index];
    }
    return BUTTON_NORMAL_STATE; // Default or error case
}

void clearButtonFlagAtIndex(int index)
{
    if (index >= 0 && index < BUTTON_COUNT) {
        buttonFlagBuffer [index] = BUTTON_NORMAL_STATE; //consider to add semaphore or mutex
    }
}

int getButtonLongPressFlagAtIndex(int index)
{
    // Return the long press flag if index is valid, otherwise return BUTTON_NORMAL_STATE
    if (index >= 0 && index < BUTTON_COUNT) {
        return buttonLongPressFlagBuffer[index];
    }
    return BUTTON_NORMAL_STATE; // Default or error case
}

void clearButtonLongPressFlagAtIndex(int index)
{
    if (index >= 0 && index < BUTTON_COUNT) {
        buttonLongPressFlagBuffer [index] = BUTTON_NORMAL_STATE; //consider to add semaphore or mutex
    }
}
