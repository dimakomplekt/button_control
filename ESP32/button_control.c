// =========================================================================================== INFO

// ESP32 library for the easy buttons control (С-File)

// Author: dimakomplekt

// Description: Button control with async_await for debounce / awaits, using pure C for embedded.
// Includes logic for buttons with / without fixation - onetime press / multiple press / 
// long-time press / infinite press.

// Instruction - at the end of the file.

// P.S. I love my namings, even if they are long. Cause i'm a user of the IDE with autofill XD.
// And this naming helps me to keep projects readable.
// But you can rename whatever you want to rename. BY AND FOR YOURSELF. 

// =========================================================================================== INFO



// =========================================================================================== IMPORT

#include "button_contol.h"
#include <assert.h>

// =========================================================================================== IMPORT


// =========================================================================================== HELPER-FUNCTIONS

// Fast read command function
// (ordinary low-code read for ESP32 without many tests)
static inline int fast_but_gpio_read(button_ctx *button)
{
    int raw_level;

    // Read
    if (button->PIN < TOTAL_PINS)
        raw_level = (GPIO.in >> button->PIN) & 0x1;
    else
        raw_level = (GPIO.in1.val >> (button->PIN - 32)) & 0x1;

    // 1 or 0 return with logic for different pull modes
    switch (button->pull_mode)
    {
        case GPIO_PULLUP_ONLY: return (raw_level == 0);         // active-low
        case GPIO_PULLDOWN_ONLY: return (raw_level == 1);       // active-high
        case GPIO_FLOATING: return raw_level;                   // choose whatever      
        case GPIO_PULLUP_PULLDOWN: return raw_level;            // choose whatever
        default: return raw_level;
    }
}

// =========================================================================================== HELPER-FUNCTIONS


// =========================================================================================== API REALIZATION


// Button constructor realization
button_ctx button_initialization(gpio_num_t PIN, gpio_pull_mode_t pull_mode, button_type type)
{
    // Check the data - assert if it's wrong

    // Pin number error handler
    if (PIN < 0 || PIN > TOTAL_PINS)
    {
        printf("Wrong pin number: %d (valid 0..%d)!\n", PIN, TOTAL_PINS);
        assert(0);
    }

    // GPIO regime error handler
    if (pull_mode != GPIO_PULLUP_ONLY &&
        pull_mode != GPIO_PULLDOWN_ONLY &&
        pull_mode != GPIO_PULLUP_PULLDOWN &&
        pull_mode != GPIO_FLOATING)
    {
        printf("You've chosen the wrong pull mode: %d\n", pull_mode);
        assert(0);
    }

    // Button type error handler
    if (type != NO_FIX && type != FIX)
    {
        printf("You've chosen the wrong button type: %d\n", type);
        assert(0);
    }


    // Fill the struct with main user data
    button_ctx new_button;

    // First fill by user data
    new_button.PIN = PIN;
    new_button.pull_mode = pull_mode;
    new_button.type = type;


    gpio_set_direction(new_button.PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(new_button.PIN, new_button.pull_mode);

    
    // First autofill of the other ctx data
    new_button.but_pressed = false;
    new_button.but_long_pressed = false;
    new_button.but_snapshot = false;

    new_button.one_time_block = false;
    new_button.mt_permission = true;

    new_button.long_time_await_end = true;

    new_button.presses_counter = 0;
    new_button.max_presses_quantity = 1;

    new_button.onetime_press_callback = NULL;

    new_button.multiple_press_callback = NULL;

    new_button.long_time_press_callback = NULL;
    new_button.long_time_press_permission = false;

    new_button.infinite_press_callback = NULL;
    new_button.infinite_press_permission = false;

    // Awaits initialization
    new_button.DEBOUNCE_AWAIT = async_await_ctx_default();
    new_button.MULTIPRESS_AWAIT = async_await_ctx_default();
    new_button.LONG_TIME_PRESS_AWAIT = async_await_ctx_default();

    // Return the new button
    return new_button; 
}



// Button control APIs realization

// Flags control

// Switch the flag value by the short BUT press (flag holds the switched value, until the BUT pressed
// once again)
void flag_control_by_but_onetime_press(button_ctx *button, bool* flag)
{
    // Error handler
    if (button->PIN == GPIO_NUM_NC) return;

    // No option to work for button with fixation
    if (button->type == FIX) return;

    // BUT state
    int but_level = fast_but_gpio_read(button);

    if (!button->but_pressed && but_level)
    {
        button->one_time_block = false; // Reset the timers for longtime check
        
        // Flag switch after debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {
            button->but_pressed = true;
            button->mt_permission = true; // Reset by the press if earlier flag blocked by long time press
        }
    }
    // Reset for the next press if but was pressed, mt_permission was obtained
    // and user don't hold the button no more.
    else if (button->but_pressed && button->mt_permission && !button->one_time_block && !but_level)
    {
        *flag = !*flag; // Change the flag
        end_await(&button->DEBOUNCE_AWAIT); // Stop await
        end_await(&button->LONG_TIME_PRESS_AWAIT);
        
        button->but_pressed = false; // Reset for the next call
        button->one_time_block = true;
    }
}


// Switch the flag value by the several BUT presses (flag holds the switched value, until the BUT pressed
// several times once again)
void flag_control_by_but_multiple_press_inside(button_ctx *button, bool* flag, uint8_t presses_quantity)
{
    // Logic error handler
    if (presses_quantity < 1) return;

    // Error handler
    if (button->PIN == GPIO_NUM_NC) return;

    // Update the maximum presses quantity value
    if (presses_quantity > button->max_presses_quantity) button->max_presses_quantity = presses_quantity;


    // BUT state read
    int but_level = fast_but_gpio_read(button);

    // One time press check
    if (!button->but_pressed && but_level)
    {
        // Flag switch after debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {
            button->mt_permission = true;

            button->but_pressed = true; // Exit from this if-condition, until the next but press

            reboot_await(&button->MULTIPRESS_AWAIT, 1, TIME_UNIT_S); // Reboot the multipress await
        }
    }
    
    if (button->but_pressed && !but_level)
    {
        button->presses_counter += 1; // Increment the presses counter
        button->but_pressed = false; // Set the permission for the next press

        end_await(&button->DEBOUNCE_AWAIT);
        end_await(&button->MULTIPRESS_AWAIT);
        end_await(&button->LONG_TIME_PRESS_AWAIT);
    }
    
    // Start multipress await only if the presses counter > 0
    if (button->presses_counter > 0 && button->mt_permission && !button->but_pressed)
    {
        async_await(&button->MULTIPRESS_AWAIT, 1, TIME_UNIT_S, true);

        if (button->MULTIPRESS_AWAIT.end_flag)
        {
            if (button->presses_counter == presses_quantity)
            {     
                *flag = !*flag; // Reverse the flag if the timer ends up
                button->presses_counter = 0;
                end_await(&button->MULTIPRESS_AWAIT);
            }
            if (button->presses_counter > button->max_presses_quantity)
            {     
                button->presses_counter = 0;
                end_await(&button->MULTIPRESS_AWAIT);
            }
        }
    }

    if (!button->mt_permission && button->long_time_await_end)
    {
        button->presses_counter = 0;
        end_await(&button->MULTIPRESS_AWAIT);
    }
}

void flag_control_by_but_multiple_press(button_ctx *button, bool* flag, uint8_t presses_quantity)
{
    if (button->PIN == GPIO_NUM_NC || presses_quantity < 1)
        return;

    // Проверяем, подходит ли текущее количество нажатий под заданное условие
    bool valid_press_count = false;

    if (presses_quantity == 1 && button->presses_counter <= 1)
        valid_press_count = true;

    else if (button->presses_counter == presses_quantity)
        valid_press_count = true;

    if (valid_press_count)
        flag_control_by_but_multiple_press_inside(button, flag, presses_quantity);
}

// Switch the flag value by the long BUT press (flag holds the switched value, until the BUT pressed
// once again)
void flag_control_by_but_longtime_press(button_ctx *button, bool* flag)
{
    // Error handler
    if (button->PIN == GPIO_NUM_NC) return;

    // No option to work for button with fixation
    if (button->type == FIX) return;


    // BUT state
    int but_level = fast_but_gpio_read(button);

    if (!button->but_long_pressed && but_level)
    {
        // Flag switch after debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {   
            button->but_long_pressed = true;
        }
    }
    // If we got the debounce flag and button still pressed
    else if (button->but_long_pressed && but_level)
    {   
        button->one_time_block = false; // Reset the timers for longtime check
        button->mt_permission = false; // Block the one time press logic

        // Wait 3 seconds
        if (async_await(&button->LONG_TIME_PRESS_AWAIT, 3, TIME_UNIT_S, false))
        {
            *flag = !*flag; // Flag switch
            button->long_time_await_end = true;
            button->one_time_block = true;
        }
    }
    if (button->long_time_await_end && but_level)
    {
        button->but_long_pressed = false;

        end_await(&button->LONG_TIME_PRESS_AWAIT);
        end_await(&button->DEBOUNCE_AWAIT);
    }

    // Reset for the next press if button was pressed and user don't hold the button no more.
    if (button->long_time_await_end && !but_level)
    {
        button->mt_permission = false;
        button->long_time_await_end = false;
    }  
    else if (!button->long_time_await_end && !but_level)
    {
        button->mt_permission = true;
        button->long_time_await_end = false;
    }  
}



// Switch the flag value by the infinite BUT press (flag holds the switched value, until the BUT pressed, 
// flag return to the first value if button ain't pressed no more)
void flag_control_by_but_infinite_press(button_ctx *button, bool* flag)
{

    // Error handler
    if (button->PIN == GPIO_NUM_NC) return;

    // BUT state
    int but_level = fast_but_gpio_read(button);

    if (!button->but_pressed && but_level)
    {
        // Save the flag value
        button->but_snapshot = *flag;

        // Flag switch after debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {
            *flag = !*flag; // Flag one time switch
            button->but_pressed = true;
            button->mt_permission = false;
        }
    }
    // Reset for the next press if button was pressed and user don't hold the button no more.
    else if (button->but_pressed && !but_level)
    {
        // Set the flag as the initial flag value
        *flag = button->but_snapshot;

        // Reset the but_press state
        button->but_pressed = false;
        
        // Stop await
        end_await(&button->DEBOUNCE_AWAIT);
    }  
}


// Callbacks control
// !!! NEED TO REBUILD LOGIC TO NEW VERSION LIKE IN THE FLAGS CONTROL !!!

// Call the callback function by the short BUT press with specified repeats quantity 
void callback_control_by_but_onetime_press(button_ctx *button, unsigned int repeats)
{
    // Error handler
    if (button->PIN == GPIO_NUM_NC) return;

    // BUT state
    int but_level = fast_but_gpio_read(button);

    if (!button->but_pressed && but_level)
    {
        // Flag switch after debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {
            button->but_pressed = true;
            button->mt_permission = true; // Reset by the press if earlier flag blocked by long time press
        }
    }
    // Reset for the next press if but was pressed, mt_permission was obtained
    // and user don't hold the button no more.
    else if (button->but_pressed && button->mt_permission && !but_level)
    {
        if (button->onetime_press_callback)
        {
            if (repeats != LOOP_PERFORMANCE)
            {
                for (unsigned int i = 0; i < repeats; i++)
                {
                    button->onetime_press_callback();
                }               
            }
            else button->onetime_press_callback();
        }

        end_await(&button->DEBOUNCE_AWAIT); // Stop await
        
        button->but_pressed = false; // Reset for the next call
    }
}


// Call the callback function by the several BUT presses with specified repeats quantity
void callback_control_by_but_multiple_press(button_ctx *button, uint8_t presses_quantity, unsigned int repeats)
{
    // Logic error handler
    if (presses_quantity < 1) return;

    // Error handler
    if (button->PIN == GPIO_NUM_NC) return;

    // BUT state
    int but_level = fast_but_gpio_read(button);

    // One time press check
    if (!button->but_pressed && but_level)
    {
        // Flag switch after debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {
            button->presses_counter += 1; // Increment the presses counter
            reboot_await(&button->MULTIPRESS_AWAIT, 3, TIME_UNIT_S); // Reboot the multipress await 
            button->but_pressed = true; // Exit from this if-condition, until the next but press
            button->mt_permission = true; // Reset by the press if earlier flag blocked by long time press
        }
    }
    // Start multipress await only if the presses counter > 0
    else if (button->presses_counter > 0)
    {
        if (async_await(&button->MULTIPRESS_AWAIT, 3, TIME_UNIT_S, true))
        {
            if (button->presses_counter == presses_quantity)
            {
                if (button->multiple_press_callback)
                {
                    if (repeats != LOOP_PERFORMANCE)
                    {
                        for (unsigned int i = 0; i < repeats; i++)
                        {
                            button->multiple_press_callback();
                        }               
                    }
                    else button->multiple_press_callback();
                }
            }
    
            // Reset the presses counter after MULTIPRESS_AWAIT ending
            button->presses_counter = 0;
        }
    }
    // Reset for the next press if but was pressed, mt_permission was obtained
    // and user don't hold the button no more.
    else if (button->but_pressed && button->mt_permission && !but_level)
    {
        end_await(&button->DEBOUNCE_AWAIT);
        end_await(&button->MULTIPRESS_AWAIT);

        button->but_pressed = false; // Set the permission for the next press
    }
} 


// Calls the callback function by the long BUT press with specified repeats quantity
void callback_control_by_but_longtime_press(button_ctx *button, unsigned int repeats)
{
    // Error handler
    if (button->PIN == GPIO_NUM_NC) return;

    // BUT state
    int but_level = fast_but_gpio_read(button);

    if (!button->but_pressed && but_level)
    {
        // Flag switch after debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {   
            button->but_pressed = true;
        }
    }
    // If we got the debounce flag and button still pressed
    else if (button->but_pressed && but_level)
    {
        // Wait 3 seconds
        if (async_await(&button->LONG_TIME_PRESS_AWAIT, 3, TIME_UNIT_S, false))
        {
            button->long_time_press_permission = true;
            button->mt_permission = false; // Block the one time press logic
        }
    }
    else if (button->long_time_press_permission)
    {
        if (button->long_time_press_callback)
        {
            if (repeats != LOOP_PERFORMANCE)
            {
                for (unsigned int i = 0; i < repeats; i++)
                {
                    button->long_time_press_callback();
                }     
                
                button->long_time_press_permission = false;
            }
            else button->long_time_press_callback();
        }
    }
    // Reset for the next press if button was pressed and user don't hold the button no more.
    else if (button->but_pressed && !but_level)
    {
        end_await(&button->LONG_TIME_PRESS_AWAIT);
        end_await(&button->DEBOUNCE_AWAIT);

        button->but_pressed = false;
    }  
}


// Calls the callback function by the long BUT press with specified repeats quantity
void callback_control_by_but_infinite_press(button_ctx *button, unsigned int repeats)
{
    // Error handler
    if (button->PIN == GPIO_NUM_NC) return;

    // BUT state
    int but_level = fast_but_gpio_read(button);

    if (!button->but_pressed && but_level)
    {
        // Flag switch after debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {   
            button->but_pressed = true;
        }
    }
    // If we got the debounce flag and button still pressed
    else if (button->but_pressed && but_level)
    {
        // Wait 3 seconds
        if (async_await(&button->LONG_TIME_PRESS_AWAIT, 3, TIME_UNIT_S, false))
        {
            button->infinite_press_permission = true;
            button->mt_permission = false; // Block the one time press logic
        }
    }
    else if (button->infinite_press_permission && but_level)
    {
        if (button->infinite_press_callback)
        {
            if (repeats != LOOP_PERFORMANCE)
            {
                for (unsigned int i = 0; i < repeats; i++)
                {
                    button->infinite_press_callback();
                }     
                
                button->infinite_press_permission = false;
            }
            else button->infinite_press_callback();
        }
    }
    // Reset for the next press if button was pressed and user don't hold the button no more.
    else if (button->but_pressed && !but_level)
    {
        button->infinite_press_permission = false;

        end_await(&button->LONG_TIME_PRESS_AWAIT);
        end_await(&button->DEBOUNCE_AWAIT);

        button->but_pressed = false;
    }  
}


// =========================================================================================== API REALIZATION


// =========================================================================================== USING EXAMPLES SECTION

// =========================================================================================== LED CONTROL BY BUTTON PRESSES

/*

// INCLUDES:
#include <stdio.h>
#include "driver/gpio.h"

#include "esp_rom_sys.h"
#include "esp_timer.h" // esp_timer_get_time()
#include "soc/rtc.h" // esp_cpu_get_cycle_count()
#include "esp_cpu.h"

#include "soc/gpio_reg.h"
#include "soc/gpio_struct.h"

#include <time.h>

#include <inttypes.h>

#include "driver/ledc.h"


// Async delay library
#include <my_libs/async_await/async_await.h>

// Button control library
#include <my_libs/button_control/button_contol.h>

// Encoder control library
#include <my_libs/encoder_control/encoder_control.h>

// PWM library
#include <my_libs/pwm/pwm_by_ledc.h>

// Random PWM library
#include <my_libs/random_pwm_setup/random_pwm_setup.h>

// I2C display library
#include "esp_log.h"
#include <outer_libs/I2C_display/ssd1306.h> // https://github.com/baoshi/ESP-I2C-OLED
#include <outer_libs/I2C_display/fonts.h>


// FreeRTOS or bare metal choose
#define USE_FREERTOS 0 // 0 for bare metal or 1 for RTOS

#define MY_BUT_VCC_1 GPIO_NUM_4


#if USE_FREERTOS

    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"

#endif
// INCLUDES END


// DEFINES:


// VARIABLES

button_ctx my_but_1;


// Button flags
bool but_1_onetime_press = false;
bool but_1_longtime_press = false;


// Display variables
char display_buf[32]; // Буфер для форматирования текста
// Display variables


// VARIABLES END


// FUNCTIONS DECLARATIONS:
void initialization();

void main_loop(void *pvParameter);

void update_display_but_onetime();
void update_display_but_longtime();
void update_display_but_no_press();

// FUNCTIONS DECLARATIONS END


// SETUP:
void initialization()
{
    // Button read ctx
    my_but_1 = button_initialization(MY_BUT_VCC_1, GPIO_PULLUP_ONLY, NO_FIX);

    if (ssd1306_init(0, 22, 21))
    {
        ESP_LOGI("OLED", "oled inited");

        ssd1306_clear(0);

        ssd1306_draw_rectangle(0, 10, 30, 20, 20, SSD1306_COLOR_WHITE);
        ssd1306_select_font(0, 0);
        ssd1306_draw_string(0, 0, 0, "glcd_5x7_font_info", SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);
        ssd1306_select_font(0, 1);
        ssd1306_draw_string(0, 0, 18, "tahoma_8pt_font_info", SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);
        ssd1306_draw_string(0, 55, 30, "Hello ESP32!", SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);

        ssd1306_refresh(0, true);
    }
    else
    {
        ESP_LOGE("OLED", "oled init failed");
    }
}
// SETUP END


// MAIN LOOP:
void main_loop(void *pvParameter)
{
    while (1)
    {
        // LOOP:

        flag_control_by_but_onetime_press(&my_but_1, &but_1_onetime_press);

        flag_control_by_but_longtime_press(&my_but_1, &but_1_longtime_press);


        if (but_1_onetime_press) 
        {   
            update_display_but_onetime();
        }

        else if (but_1_longtime_press) 
        {
            but_1_onetime_press = false;

            update_display_but_longtime();
        }
        else if (!but_1_longtime_press && !but_1_onetime_press) 
        {
            update_display_but_no_press();
        }
    
        // LOOP END

        // Delay for correct loop
        #if USE_FREERTOS
            // vTaskDelay(pdMS_TO_TICKS(RTOS_LOOP_DELAY_MS));
        #else
            // esp_rom_delay_us(1000);

            await(1, TIME_UNIT_US); // Ending_await
        #endif
    }
}
// MAIN LOOP END


// MAIN:
void app_main()
{
    // UART setup for serial monitor 

    // INITIALIZATION:
    initialization();
    // INITIALIZATION END

    // PRE-CYCLE:

    // PRE-CYCLE END

    // CYCLE:
    #if USE_FREERTOS
        xTaskCreate(main_loop, "Main Loop", 2048, NULL, 5, NULL);
    #else
        main_loop(NULL);
    #endif
    // CYCLE END
}
// MAIN END


// FUNCTIONS DEFINITIONS:


// Display update
void update_display_but_no_press()
{
    ssd1306_clear(0);

    ssd1306_select_font(0, 1);

    snprintf(display_buf, sizeof(display_buf), "Button ain't pressed!");
    ssd1306_draw_string(0, 0, 0, display_buf, SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);

    snprintf(display_buf, sizeof(display_buf), "Total presses: %u", my_but_1.presses_counter);
    ssd1306_draw_string(0, 0, 30, display_buf, SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);

    ssd1306_refresh(0, true);
}


void update_display_but_onetime()
{
    ssd1306_clear(0);

    ssd1306_select_font(0, 1);

    snprintf(display_buf, sizeof(display_buf), "Button pressed by onetime!");
    ssd1306_draw_string(0, 0, 0, display_buf, SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);

    snprintf(display_buf, sizeof(display_buf), "Total presses: %u", my_but_1.presses_counter);
    ssd1306_draw_string(0, 0, 30, display_buf, SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);

    ssd1306_refresh(0, true);
}


void update_display_but_longtime()
{
    ssd1306_clear(0);

    ssd1306_select_font(0, 1);

    snprintf(display_buf, sizeof(display_buf), "Button pressed by longtime!");
    ssd1306_draw_string(0, 0, 0, display_buf, SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);

    snprintf(display_buf, sizeof(display_buf), "Total presses: %u", my_but_1.presses_counter);
    ssd1306_draw_string(0, 0, 30, display_buf, SSD1306_COLOR_WHITE, SSD1306_COLOR_BLACK);

    ssd1306_refresh(0, true);
}

// FUNCTIONS DEFINITIONS END

*/

// =========================================================================================== LED CONTROL BY BUTTON PRESSES

// =========================================================================================== USING EXAMPLES SECTION
