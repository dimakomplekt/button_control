# button_control
A lightweight button management library for embedded systems (ESP32, STM32, Arduino). It provides easy handling of different button presses, including short press, long press, and press-and-hold, with flag-based and callback-based control.



✨ Features

* Minimalistic button press detection API
* Supports short press, long press, and combined logic
* Handles asynchronous debouncing internally
* Provides flags for state checking and optional callbacks
* Compatible with multiple embedded platforms
* Fully non-blocking, no threads or RTOS required



🚀 Basic Idea

* The library lets you handle button presses without blocking the main loop.
* Each button has its own context structure storing state, debounce info, and press duration.
* Debounce is asynchronous, so multiple buttons can be handled in parallel
* Flags are updated automatically for one-time short presses or long presses
* Callbacks can be optionally assigned for reactive handling



⚠️ BIG WARNING ⚠️

You must download and link the async_await library from "https://github.com/dimakomplekt/async_await",
because the button library relies on its timing engine for asynchronous debouncing!

⚠️ BIG WARNING ⚠️



❓ How It Works

* Each button has a context (button_ctx)
* Presses are detected via polling in the main loop
* Debounce is handled internally and asynchronously using async_await
* Flags are set for short press, long press, and state changes
* Optional callbacks can be assigned to react immediately to events



⚡ Using Examples - AT THE END OF C-FILES


Main API for initialization looks like:

```c
#include <my_libs/button_control/button_contol.h>

// Button context
button_ctx my_button;

// Pin definition
#define BTN_PIN GPIO_NUM_0

// Initialize button with internal pullup and no fix
my_button = button_initialization(BTN_PIN, GPIO_PULLUP_ONLY, NO_FIX);

// Controlled flags 
bool short_pressed_logic_flag = false;
bool long_pressed_logic_flag  = false;
```


Main API for control looks like:

```c
flag_control_by_but_onetime_press(&my_button, &short_pressed_logic_flag);

if (short_pressed_logic_flag)
{
    // Your logic
    short_pressed_logic_flag = false;
}

// Long press detection
flag_control_by_but_longtime_press(&my_button, &long_pressed_logic_flag);

if (long_pressed_logic_flag)
{
    // Your logic
    long_pressed_logic_flag = false;
}
```



🫟 Current Version - Version: 0.9



⚠️ Known limitations:

  * Debugging required for simultaneous processing of different press types
  * Error handling and callbacks need further implementation
  * Some edge cases in fast repeated presses not fully covered



🛠 Future Plans

  * Finalize callback system for short and long presses
  * Improve error handling and reporting
  * Enhance multi-button support with overlapping press types
  * Maintain full async compatibility with async_await



🫵 You are welcome to help us make this library better!
