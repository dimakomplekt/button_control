// =========================================================================================== IMPORT

#include "button_control.h"
#include <assert.h>

// =========================================================================================== IMPORT


// =========================================================================================== HELPER-FUNCTIONS

// =========================================================================================== DECLARATIONS

void flag_control_by_but_multiple_press_inside(button_ctx *button, bool* flag, uint8_t presses_quantity);

// =========================================================================================== DECLARATIONS


// =========================================================================================== REALIZATIONS

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

// =========================================================================================== REALIZATIONS

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

    new_button.mt_permission = true;

    new_button.long_time_await_end = true;
    new_button.one_time_await_end = true;

    new_button.presses_counter = 0;
    new_button.max_presses_quantity = 1;

    new_button.onetime_press_callback = NULL;

    new_button.multiple_press_callback = NULL;

    new_button.long_time_press_callback = NULL;
    new_button.long_time_press_permission = false;

    new_button.infinite_press_callback = NULL;
    new_button.infinite_press_permission = false;


    // 1st call error handlers flags initialization / reinitialization
    new_button.error_handlers_ctx.onetime_press_flag_control_first_call = true;
    new_button.error_handlers_ctx.multiple_press_flag_control_first_call = true;
    new_button.error_handlers_ctx.longtime_press_flag_control_first_call = true;
    new_button.error_handlers_ctx.infinite_press_flag_control_first_call = true;
    new_button.error_handlers_ctx.onetime_press_callback_control_first_call = true;
    new_button.error_handlers_ctx.multiple_press_callback_control_first_call = true;
    new_button.error_handlers_ctx.longtime_press_callback_control_first_call = true;
    new_button.error_handlers_ctx.infinite_press_callback_control_first_call = true;


    // Awaits initialization
    new_button.DEBOUNCE_AWAIT = async_await_ctx_default();
    new_button.MULTIPRESS_AWAIT = async_await_ctx_default();
    new_button.LONG_TIME_PRESS_AWAIT = async_await_ctx_default();

    // Return the new button
    return new_button; 
}



// =========================================================================================== Button control APIs realization


// =========================================================================================== Helper-functions for button error handling

bool button_context_check(button_ctx *button)
{
    bool check_status = true;

    // Button ctx initialization check
    if (button == NULL)
    {
        fprintf(stderr, "[ERROR] Button context at %p is NULL!\n", (void*)button);
        check_status = false;
    }
    // Button ctx PIN number check
    if (button->PIN == GPIO_NUM_NC)
    {
        fprintf(stderr, "[ERROR] Button context at %p has unitialized PIN!\n", (void*)button);
        check_status = false;
    }

    return check_status;
}


bool button_type_check(button_ctx *button, button_type expected_type)
{
    bool check_status = true;

    // Expected button type check
    if (button->type != expected_type)
    {
        fprintf(stderr, "[ERROR] Button context at %p has wrong type! Expected: %d, Actual: %d\n", 
            (void*)(button), expected_type, button->type);

        check_status = false;
    }

    return check_status;
}

// =========================================================================================== Helper-functions for button error handling


// =========================================================================================== Flags control


// Switch the flag value by the short BUT press (flag holds the switched value, until the BUT pressed
// once again)
void flag_control_by_but_onetime_press(button_ctx *button, bool* flag)
{
    // First call error handler
    if (button->error_handlers_ctx.onetime_press_flag_control_first_call == true)
    {
        // Wrong button ctx 
        if (!button_context_check(button)) return;
        // Wrong button type (onetime_press only for NO_FIX buttons)
        if (!button_type_check(button, NO_FIX)) return;

        button->error_handlers_ctx.onetime_press_flag_control_first_call = false;
    }

    // Simultanious work with multiple press block
    if (!button->error_handlers_ctx.multiple_press_flag_control_first_call ||
        !button->error_handlers_ctx.multiple_press_callback_control_first_call)
    {
        fprintf(stderr, "[ERROR] button_ctx at %p: onetime_press and  multipress control called simultaniously!\n", (void*)button);
        assert(0);
    }

    // Simultanious work with infinite press block
    if (!button->error_handlers_ctx.infinite_press_flag_control_first_call ||
        !button->error_handlers_ctx.infinite_press_callback_control_first_call)
    {
        fprintf(stderr, "[ERROR] button_ctx at %p: onetime and infinite press control called simultaniously!\n", (void*)button);
        assert(0);
    }
    

    // BUT state
    int but_level = fast_but_gpio_read(button);

    if (!button->but_pressed && but_level)
    {        
        // Flag switch after debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {
            // Reset block flags after the new press
            button->one_time_await_end = false;
            button->long_time_await_end = false;

            button->but_pressed = true;

            end_await(&button->DEBOUNCE_AWAIT);
        }
    }
    // Reset for the next press if but was pressed
    // and user don't hold the button no more.
    if (button->but_pressed && !button->long_time_await_end && !but_level)
    {
        *flag = !*flag; // Change the flag
        
        // Block the longtime press flag switch after onetime press
        button->one_time_await_end = true;

        // Stop awaits
        end_await(&button->DEBOUNCE_AWAIT);
        end_await(&button->LONG_TIME_PRESS_AWAIT);
    }

    if (button->one_time_await_end)
    {
        // Reset ctx flags for the next call
        button->but_pressed = false;
        button->but_long_pressed = false;
    }
}

void flag_control_by_but_multiple_press(button_ctx *button, bool* flag, uint8_t presses_quantity)
{
    // First call error handler
    if (button->error_handlers_ctx.multiple_press_flag_control_first_call == true)
    {
        // Wrong button ctx 
        if (!button_context_check(button)) return;
        // Wrong button type (onetime_press only for NO_FIX buttons)
        if (!button_type_check(button, NO_FIX)) return;

        button->error_handlers_ctx.multiple_press_flag_control_first_call = false;
    }

    // Simultanious work with onetime press block
    if (!button->error_handlers_ctx.onetime_press_flag_control_first_call ||
        !button->error_handlers_ctx.onetime_press_callback_control_first_call)
    {
        fprintf(stderr, "[ERROR] button_ctx at %p: multipress and onetime_press control called simultaniously!\n", (void*)button);
        assert(0);
    }

    // Simultanious work with infinite press block
    if (!button->error_handlers_ctx.infinite_press_flag_control_first_call ||
        !button->error_handlers_ctx.infinite_press_callback_control_first_call)
    {
        fprintf(stderr, "[ERROR] button_ctx at %p: multiple and infinite press control called simultaniously!\n", (void*)button);
        assert(0);
    }
    

    // Check if the current presses counter value is valid for the requested presses quantity
    // This algorithm calls the inside function (and the main multipress flag switch logic) only for current press_quantity
    // This allows to call several multipress flag controls with different press quantities on the same button
    bool valid_press_count = false;

    if (presses_quantity == 1 && button->presses_counter == 0)
        valid_press_count = true;

    else if (button->presses_counter == presses_quantity)
        valid_press_count = true;

    if (valid_press_count)
        flag_control_by_but_multiple_press_inside(button, flag, presses_quantity);
}



// Switch the flag value by the several BUT presses (flag holds the switched value, until the BUT pressed
// several times once again)
void flag_control_by_but_multiple_press_inside(button_ctx *button, bool* flag, uint8_t presses_quantity)
{
    // Logic error handler
    if (presses_quantity < 1) return;

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
            // Reset block flags after the new press
            button->multipress_await_end = false;
            button->long_time_await_end = false;

            button->but_pressed = true; // Exit from this if-condition, until the next but press

            reboot_await(&button->MULTIPRESS_AWAIT, 1, TIME_UNIT_S); // Reboot the multipress await

            end_await(&button->DEBOUNCE_AWAIT);
        }
    }
    
    if (button->but_pressed && !but_level)
    {
        button->presses_counter += 1; // Increment the presses counter
        button->but_pressed = false; // Set the permission for the next press

        end_await(&button->MULTIPRESS_AWAIT);
        end_await(&button->LONG_TIME_PRESS_AWAIT);
    }
    
    // Start multipress await only if the presses counter > 0
    if (button->presses_counter > 0 && !button->but_pressed && !button->long_time_await_end)
    {
        async_await(&button->MULTIPRESS_AWAIT, 1, TIME_UNIT_S, true);

        // If the multipress timer ends up
        if (button->MULTIPRESS_AWAIT.end_flag)
        {
            button->multipress_await_end = true;

            // Press quanity coincidence case
            if (button->presses_counter == presses_quantity)
            {     
                *flag = !*flag; // Reverse the flag, choosen for the presses quantity

                // Stop awaits
                end_await(&button->LONG_TIME_PRESS_AWAIT);
                end_await(&button->MULTIPRESS_AWAIT);

                button->presses_counter = 0;
            }
            // Overflow protection
            if (button->presses_counter > button->max_presses_quantity)
            {     
                // stop awaits when aborting multipress sequence
                end_await(&button->LONG_TIME_PRESS_AWAIT);
                end_await(&button->MULTIPRESS_AWAIT);

                button->presses_counter = 0;
            }

            // Else - no actions
        }
    }

    // Reset by the longtime press block
    if (button->long_time_await_end)
    {
        button->presses_counter = 0;
        end_await(&button->MULTIPRESS_AWAIT);
    }

    // Reset for the next press if multipress ends and user don't hold the button no more.
    else if (button->multipress_await_end)
    {
        button->but_pressed = false;
        button->but_long_pressed = false;

        button->one_time_await_end = false;
        button->multipress_await_end = false;
    }
}


// Switch the flag value by the long BUT press (flag holds the switched value, until the BUT pressed
// once again)
void flag_control_by_but_longtime_press(button_ctx *button, bool* flag)
{
    // First call error handler
    if (button->error_handlers_ctx.longtime_press_flag_control_first_call == true)
    {
        // Wrong button ctx 
        if (!button_context_check(button)) return;
        // Wrong button type (onetime_press only for NO_FIX buttons)
        if (!button_type_check(button, NO_FIX)) return;

        button->error_handlers_ctx.longtime_press_flag_control_first_call = false;
    }

    // Simultanious work with infinite press block
    if (!button->error_handlers_ctx.infinite_press_flag_control_first_call ||
        !button->error_handlers_ctx.infinite_press_callback_control_first_call)
    {
        fprintf(stderr, "[ERROR] button_ctx at %p: longtime and infinite press control called simultaniously!\n", (void*)button);
        assert(0);
    }


    // BUT state
    int but_level = fast_but_gpio_read(button);


    if (!button->but_long_pressed && but_level)
    {
        // Flag switch after debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {   
            // Reset block flags after the new press
            button->one_time_await_end = false;
            button->multipress_await_end = false;
            button->long_time_await_end = false;

            button->but_long_pressed = true;

            end_await(&button->DEBOUNCE_AWAIT);
        }
    }
    // If we got the debounce flag and button still pressed
    else if (button->but_long_pressed && but_level)
    {   
        // Long press already handled, do nothing until button release
        if (button->long_time_await_end) return;

        // Additional await reboot for the instant repeat blocking
        if (!button->LONG_TIME_PRESS_AWAIT.initialization_status) reboot_await(&button->LONG_TIME_PRESS_AWAIT, 3, TIME_UNIT_S);

        // Wait 3 seconds
        // Makes &button->LONG_TIME_PRESS_AWAIT.initialization_status equal to true by the first call and blocks upper function calls in loop
        // until the await ends up
        if (async_await(&button->LONG_TIME_PRESS_AWAIT, 3, TIME_UNIT_S, false))
        {
            if (!button->one_time_await_end && !button->multipress_await_end)
            {
                *flag = !*flag; // Flag switch

                // Block the onetime press and multipress press flag switch after longtime press end
                button->long_time_await_end = true;

                // Stop awaits 
                end_await(&button->MULTIPRESS_AWAIT);
                end_await(&button->LONG_TIME_PRESS_AWAIT); // Makes &button->LONG_TIME_PRESS_AWAIT.initialization_status equal to false
            }
        }
    }

    // Block the onetime and multipress press flag switch after longtime press end with button still pressed
    if (button->long_time_await_end && but_level)
    {
        button->one_time_await_end = false;
        button->multipress_await_end = false;
    }

    // Reset for the next press if button was pressed and user don't hold the button no more.
    else if (button->long_time_await_end && !but_level)
    {
        button->but_pressed = false;
        button->but_long_pressed = false;
    }
}


// Switch the flag value by the infinite BUT press (flag holds the switched value, until the BUT pressed, 
// flag return to the first value if button ain't pressed no more)
void flag_control_by_but_infinite_press(button_ctx *button, bool* flag)
{
    // First call error handler
    if (button->error_handlers_ctx.infinite_press_flag_control_first_call == true)
    {
        // Wrong button ctx 
        if (!button_context_check(button)) return;

        // Could work with any button type
        if (button->type != NO_FIX && button->type != FIX)
        {
            fprintf(stderr, "[ERROR] Button context at %p has wrong type! Expected: %d or %d, Actual: %d\n",
                (void*)(button), NO_FIX, FIX, button->type);

            return;
        }

        button->error_handlers_ctx.infinite_press_flag_control_first_call = false;
    }

    // Simultanious work with onetime press block
    if (!button->error_handlers_ctx.onetime_press_flag_control_first_call ||
        !button->error_handlers_ctx.onetime_press_callback_control_first_call)
    {
        fprintf(stderr, "[ERROR] button_ctx at %p: infinite and onetime_press control called simultaniously!\n", (void*)button);
        assert(0);
    }

    // Simultanious work with multiple press block
    if (!button->error_handlers_ctx.multiple_press_flag_control_first_call ||
        !button->error_handlers_ctx.multiple_press_callback_control_first_call)
    {
        fprintf(stderr, "[ERROR] button_ctx at %p: infinite and  multipress control called simultaniously!\n", (void*)button);
        assert(0);
    }

    // Simultanious work with longtime press block
    if (!button->error_handlers_ctx.longtime_press_flag_control_first_call ||
        !button->error_handlers_ctx.longtime_press_callback_control_first_call)
    {
        fprintf(stderr, "[ERROR] button_ctx at %p: infinite and longtime press control called simultaniously!\n", (void*)button);
        assert(0);
    }

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


// =========================================================================================== Flags control

// =========================================================================================== Callbacks control

/*

// Call the callback function by the short BUT press with specified repeats quantity 
void callback_control_by_but_onetime_press(button_ctx *button, unsigned int repeats)
{
    // Error handler
    if (button->PIN == GPIO_NUM_NC) return;

    // No option to work for button with fixation
    if (button->type == FIX) return;

    // BUT state
    int but_level = fast_but_gpio_read(button);

    if (!button->but_pressed && but_level)
    {
        // button->one_time_block = false; // Reset the timers for longtime check

        // Callback after debounce
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
        if (button->onetime_press_callback)
        {
            if (repeats != LOOP_PERFORMANCE)
                for (unsigned int i = 0; i < repeats; i++)
                    button->onetime_press_callback();
                    
            else
                button->onetime_press_callback();
        }

        end_await(&button->DEBOUNCE_AWAIT); // Stop await
        end_await(&button->LONG_TIME_PRESS_AWAIT);

        button->but_pressed = false; // Reset for the next call
        // button->one_time_block = true;
    }
}


// Call the callback function by the several BUT presses with specified repeats quantity
void callback_control_by_but_multiple_press(button_ctx *button, uint8_t presses_quantity, unsigned int repeats)
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
        // Debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {
            button->mt_permission = true;
            button->but_pressed = true; // Exit from this if-condition, until the next but press

            reboot_await(&button->MULTIPRESS_AWAIT, 1, TIME_UNIT_S); // Reboot the multipress await
        }
    }

    // On release increment counter
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
                if (button->multiple_press_callback)
                {
                    if (repeats != LOOP_PERFORMANCE)
                        for (unsigned int i = 0; i < repeats; i++)
                            button->multiple_press_callback();
                    else
                        button->multiple_press_callback();
                }

                // stop all related awaits when action executed
                end_await(&button->DEBOUNCE_AWAIT);
                end_await(&button->LONG_TIME_PRESS_AWAIT);
                end_await(&button->MULTIPRESS_AWAIT);

                // Reset the presses counter after MULTIPRESS_AWAIT ending
                button->presses_counter = 0;
            }
            if (button->presses_counter > button->max_presses_quantity)
            {
                // abort sequence: stop awaits
                end_await(&button->DEBOUNCE_AWAIT);
                end_await(&button->LONG_TIME_PRESS_AWAIT);
                end_await(&button->MULTIPRESS_AWAIT);

                button->presses_counter = 0;
            }
        }
    }

    if (!button->mt_permission && button->long_time_await_end)
    {
        button->presses_counter = 0;
        end_await(&button->MULTIPRESS_AWAIT);
    }
}


// Calls the callback function by the long BUT press with specified repeats quantity
void callback_control_by_but_longtime_press(button_ctx *button, unsigned int repeats)
{
    // Error handler
    if (button->PIN == GPIO_NUM_NC) return;

    // No option to work for button with fixation
    if (button->type == FIX) return;

    // BUT state
    int but_level = fast_but_gpio_read(button);

    if (!button->but_long_pressed && but_level)
    {
        // Debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {
            button->but_long_pressed = true;
        }
    }
    // If we got the debounce flag and button still pressed
    else if (button->but_long_pressed && but_level)
    {
        // button->one_time_block = false; // Reset the timers for longtime check
        button->mt_permission = false; // Block the one time press logic

        // Wait LONG press time
        if (async_await(&button->LONG_TIME_PRESS_AWAIT, 3, TIME_UNIT_S, false))
        {
            button->long_time_press_permission = true;
            button->long_time_await_end = true;
            // button->one_time_block = true;
        }
    }

    if (button->long_time_await_end && but_level)
    {
        button->but_long_pressed = false;

        end_await(&button->LONG_TIME_PRESS_AWAIT);
        end_await(&button->DEBOUNCE_AWAIT);
    }

    // Execute callback when permission is granted
    if (button->long_time_press_permission)
    {
        if (button->long_time_press_callback)
        {
            if (repeats != LOOP_PERFORMANCE)
                for (unsigned int i = 0; i < repeats; i++)
                    button->long_time_press_callback();
            else
                button->long_time_press_callback();

            // stop awaits after handling long press action
            end_await(&button->LONG_TIME_PRESS_AWAIT);
            end_await(&button->DEBOUNCE_AWAIT);
            end_await(&button->MULTIPRESS_AWAIT);

            button->long_time_press_permission = false;
        }
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


// Calls the callback function by the infinite BUT press with specified repeats quantity
void callback_control_by_but_infinite_press(button_ctx *button, unsigned int repeats)
{
    // Error handler
    if (button->PIN == GPIO_NUM_NC) return;

    // BUT state
    int but_level = fast_but_gpio_read(button);

    if (!button->but_pressed && but_level)
    {
        // Save the flag value snapshot (not used for callback but keep behaviour)
        button->but_snapshot = false;

        // Debounce
        if (async_await(&button->DEBOUNCE_AWAIT, 3, TIME_UNIT_MS, false))
        {
            button->but_pressed = true;
        }
    }
    // If we got the debounce flag and button still pressed
    else if (button->but_pressed && but_level)
    {
        // Wait LONG press time
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
                for (unsigned int i = 0; i < repeats; i++)
                    button->infinite_press_callback();
            else
                button->infinite_press_callback();

            // stop awaits that were used to detect and allow infinite action
            end_await(&button->LONG_TIME_PRESS_AWAIT);
            end_await(&button->DEBOUNCE_AWAIT);

            button->infinite_press_permission = false;
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

*/

// =========================================================================================== Callbacks control

// =========================================================================================== 

// =========================================================================================== API REALIZATION
