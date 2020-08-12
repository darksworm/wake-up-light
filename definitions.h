#pragma once

#define LCD_ENABLE A1
#define LCD_D4 A2
#define LCD_D5 A3
#define LCD_D6 A4
#define LCD_D7 A5
#define LCD_RS A0
#define LCD_BACKLIGHT 7

#define BTN_BLUE 11
#define BTN_RED 12
#define BTN_WHITE 10
#define BTN_GREEN 13 
#define BTN_COUNT 4 
#define RELAY2 9 
#define RELAY 8
#define HIGH1 4
#define LOW2 6
#define LOW1 5

#define DEBOUNCE_DELAY 200

#define ENABLED_ADDR 0
#define START_MINUTES_ADDR 1
#define RAMP_UP_DURATION_MINUTES_ADDR 2
#define END_MINUTES_ADDR 3

#define MAGIC_NUMBER 137

enum class ClockState
{
    DISABLED,
    ACTIVE_TIMER,
    LIGHTS_ON,
    VARIABLE_SELECTION,
    CHANGING_VARIABLE
};

enum class ClockVariable
{
    DISABLE_CLOCK,
    START_TIME,
    RAMP_UP_TIME,
    END_TIME
};

enum Button {
    BLUE = 0,
    RED = 1,
    WHITE = 2,
    GREEN = 3
};

struct State {
    // current clock state
    ClockState current_clock_state = ClockState::ACTIVE_TIMER;
    // previous clock state (used when exiting temporary states)
    ClockState prev_clock_state = ClockState::ACTIVE_TIMER;

    // current variable displayed on lcd
    ClockVariable current_clock_variable = ClockVariable::DISABLE_CLOCK;

    // calendar number of day to skip
    int skip_day = -1;

    // previous button states (used for checking whether a change happened)
    int prev_button_state[BTN_COUNT] = {0, 0, 0, 0};
    // current button states
    int curr_button_state[BTN_COUNT] = {0, 0, 0, 0};
    // when the last debounce happened in ms (used for debounce timer)
    int last_debounce_start = 0;

    // when the lights should start ramping up in minutes (7:20 AM = 7 * 60 + 20)
    int start_time_minutes;
    // for how long the lights should be ramping up to 100% for in minutes
    int ramp_up_duration_minutes;
    // when the lights should turn off in minutes (7:20 AM = 7 * 60 + 20)
    int end_time_minutes;
};

// Indermediary state variable representation in the EEPROM
// This is a separate struct, because the data is stored differently
// in the EEPROM than used in the state.
struct EEPROMState {
    // minutes are stored in the EEPROM divided by 10, so that one days minutes
    // fit in to a single byte. (1440 mins = 144, which is < 255)
    int start_time_minutes;
    int ramp_up_duration_minutes;
    int end_time_minutes;

    int clock_enabled;
};