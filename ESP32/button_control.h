// =========================================================================================== INFO

// ESP32 library for the easy buttons control (Header File, C version)

// Author: dimakomplekt

// Description: Button control with async_await for debounce / awaits, using pure C for embedded.
// Includes logic for buttons with / without fixation - onetime press / multiple press / 
// long-time press / infinite press.

// Instruction - at the end of the file.

// P.S. I love my namings, even if they are long. Cause i'm a user of the IDE with autofill XD.
// And this naming helps me to keep projects readable.
// But you can rename whatever you want to rename. BY AND FOR YOURSELF. 

// =========================================================================================== INFO

#ifndef BUTTON_CONTROL_H
#define BUTTON_CONTROL_H

// =========================================================================================== IMPORT

#include <stdbool.h>

#include "driver/gpio.h"                        // For PIN enum types - gpio_num_t 
#include "hal/gpio_types.h"                     // For PIN pull mode enum types - gpio_pull_mode_t  

// For low-code read operation
#include "soc/gpio_reg.h" 
#include "soc/gpio_struct.h"

#include <my_libs/async_await/async_await.h>    // Async await lib connection

// =========================================================================================== IMPORT


// =========================================================================================== DEFINES

#define LOOP_PERFORMANCE ((unsigned int)-1)     // Define for easy infinite callbacks performance 
#define TOTAL_PINS 35                           // Total GPIOs quantity on your board
 
// =========================================================================================== DEFINES


// =========================================================================================== EXT CONST


// =========================================================================================== EXT CONST


// =========================================================================================== EXT VAR


// =========================================================================================== EXT VAR


// =========================================================================================== EXT ENUMS

// Button types structure
typedef enum {

    NO_FIX,     // Button without fixation / sensor-button: ONETIME, MULTIPLE, LONG-TIME, INFINITE PRESS
    FIX,        // Button with fixation: ONLY INFINITE PRESS

} button_type;

// =========================================================================================== EXT ENUMS



// =========================================================================================== EXT STRUCTS

// Button structure
typedef struct
{
    gpio_num_t PIN;                                 // Pin for button control
    gpio_pull_mode_t pull_mode;                     // Control type (pullup / pulldown)

    button_type type;                               // Button type

    bool but_pressed;                               // Flag for button press state control
    bool but_long_pressed;                          // Flag for button press state control
    bool but_snapshot;                              // Flag for button long-time pressing control

    bool one_time_block;                            // Flag for longtime-time press block from onetime press 
    bool mt_permission;                       // Flag for one-time press block from longtime press  

    bool long_time_await_end;

    unsigned int presses_counter;                   // Variable for presses quantity control
    uint8_t max_presses_quantity;                   // Variable for reset logic

    void (*onetime_press_callback)(void);           // Callback function for onetime press

    void (*multiple_press_callback)(void);          // Callback function for multiple press

    void (*long_time_press_callback)(void);         // Callback function for long_time press
    bool long_time_press_permission;                // Flag for the callback use

    void (*infinite_press_callback)(void);          // Callback function for infinite press
    bool infinite_press_permission;                 // Flag for the callback use

    async_await_ctx DEBOUNCE_AWAIT;                 // Async await context for debounce await
    async_await_ctx MULTIPRESS_AWAIT;               // Async await context for multipress await reset
    async_await_ctx LONG_TIME_PRESS_AWAIT;          // Async await context for multipress await reset

} button_ctx;


// =========================================================================================== EXT STRUCTS


// =========================================================================================== API


// Function: button
// Button ctx constructor.
// Call in the initialization zone for the button initialization.
// Call as: button_ctx button_1 = button_initialization(BUT_PIN, GPIO_PULLUP_ONLY, NO_FIX);
button_ctx button_initialization(gpio_num_t PIN, gpio_pull_mode_t pull_mode, button_type type);


// Function: flag_control_by_but_onetime_press
// Purpose: Reverse the flag parameter bool value by the short button press and save this flag state
// by the selected button and flag with debounce async await. 
//
// !!! WARNING !!!
//
// Can't be used with multipress logic (use flag_control_by_but_onetime_press(&button_1, my_flag, 1)
// instead)
//
// !!! WARNING !!!
//
// Call as: flag_control_by_but_onetime_press(&button_1, my_flag);
// Than check the flag in if-else, like: if (my flag) { ... }
void flag_control_by_but_onetime_press(button_ctx *button, bool* flag);


// Function: flag_control_by_but_onetime_press
// Purpose: Reverse the flag parameter bool value by the short button press and save this flag state
// by the selected button and flag with debounce async await. 
// Call as: flag_control_by_but_onetime_press(&button_1, my_flag, 3);
// Than check the flag in if-else, like: if (my flag) { ... }
// You are able to check several flags by several multipress controls with DIFFERENT presses_quantity 
void flag_control_by_but_multiple_press(button_ctx *button, bool* flag, uint8_t presses_quantity);

void multitime_press_counter_control(button_ctx *button);

// Function: flag_control_by_but_onetime_press
// Purpose: Reverse the flag parameter bool value by the longtime button press (3 seconds) and save this flag state
// by the selected button and flag with debounce and press timer async await.
//
// !!! WARNING !!!
//
// Can't be used with flag_control_by_but_infinite_press
//
// !!! WARNING !!!
// 
// Call as: flag_control_by_but_onetime_press(&button_1, my_flag);
// Than check the flag in if-else, like: if (my flag) { ... }
void flag_control_by_but_longtime_press(button_ctx *button, bool* flag);


// Function: flag_control_by_but_infinite_press
// Purpose: Reverse the flag parameter bool value by the endless button press and return the
// initial state without the pressing, by the selected button and flag with debounce async await 
//
// !!! WARNING !!!
//
// Can't be used with flag_control_by_but_longtime_press
//
// !!! WARNING !!!
// 
// Call as: flag_control_by_but_infinite_press(&button_1, my_flag);В
// Than check the flag in if-else, like: if (my flag) { ... }
void flag_control_by_but_infinite_press(button_ctx *button, bool* flag);



// Function: callback_control_by_but_onetime_press
// Purpose:  loop / multiple perform the void function by the short button press.
// Works by the selected button and repeats value with debounce async await. 
// For the function, passed, by command like: button_1.onetime_press_callback = function_1;
// Call as: callback_control_by_but_onetime_press(&button_1, 5);
void callback_control_by_but_onetime_press(button_ctx *button, unsigned int repeats);


// Function: callback_control_by_but_multiple_press
// Purpose: loop / multiple perform the void function by the multiple button press.
// Works by the selected button, repeats value and presses quantity with debounce async await. 
// For the function, passed, by command like: button_1.multiple_press_callback = function_1;
// Call as: callback_control_by_but_multiple_press(&button_1, 5, LOOP_PERFORMANCE);
void callback_control_by_but_multiple_press(button_ctx *button, uint8_t presses_quantity, unsigned int repeats);


// Function: callback_control_by_but_longtime_press
// Purpose: loop / multiple perform the void function by the long button press.
// Works by the selected button and repeats value with debounce async await. 
// For the function, passed, by command like: button_1.longtime_press_callback = function_1;
// Call as: callback_control_by_but_longtime_press(&button_1, LOOP_PERFORMANCE);
void callback_control_by_but_longtime_press(button_ctx *button, unsigned int repeats);


// Function: callback_control_by_but_infinite_press
// Purpose: infinite or several times perform the void function by the infinite button press.
// drop the performance if the button is no longer pressed
// Works by the selected button with debounce async await. 
// For the function, passed, by command like: button_1.longtime_press_callback = function_1;
// Call as: callback_control_by_but_longtime_press(&button_1, 2);
void callback_control_by_but_infinite_press(button_ctx *button, unsigned int repeats);


// =========================================================================================== API


#endif // BUTTON_CONTROL_H

// =========================================================================================== INSTRUCTION



// =========================================================================================== INSTRUCTION
