#pragma once

#define LCD_ENABLE A1
#define LCD_D4 A2
#define LCD_D5 A3
#define LCD_D6 3
#define LCD_D7 2
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

#define DEBOUNCE_DELAY 150

#define MAGIC_NUMBER_ADDR 0
#define MAGIC_NUMBER 137
#define STATE_ADDR 9

enum class ClockState
{
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
    END_TIME,
    AUTO_ADJUST_DURATION,
    TARGET_START_TIME,
};

enum class ButtonColor
{
    BLUE,
    RED,
    WHITE,
    GREEN
};

#define STORABLE_STATE_VARIABLES(prefix, suffix) \
    /* when the lights should start ramping up in minutes (7:20 AM = 7 * 60 + 20)*/ \
    prefix start_time_minutes suffix \
    /* for how long the lights should be ramping up to 100% for in minutes */ \
    prefix ramp_up_duration_minutes suffix \
    /* when the lights should turn off in minutes (7:20 AM = 7 * 60 + 20) */ \
    prefix end_time_minutes suffix \
    /* whether to disable the lights clock */ \
    prefix clock_is_disabled suffix \
    /* daily start time adjustment in minutes */ \
    prefix auto_adjust_duration_daily_minutes suffix \
    /* what start time we should be adjusting towards */ \
    prefix target_start_time_minutes suffix \
    /* on which day the last adjustment took place */ \
    prefix last_adjustment_day suffix

int eeprom_default_state_vals[] = {
    7 * 60 + 30, // start at 7:30AM
    30,          // ramp for 30min
    8 * 60 + 30, // end at 8:30AM
    0,           // clock enabled
    0,           // no auto adjustment
    7 * 60 + 30, // target start time same as start time
    32           // adjust on non-existant day
};

struct State
{
    // current clock state
    ClockState current_clock_state = ClockState::ACTIVE_TIMER;

    // current variable displayed on lcd
    ClockVariable current_clock_variable = ClockVariable::DISABLE_CLOCK;

    // calendar number of day to skip
    int skip_day = -1;

    // last brightness step
    int last_step = 0;

    // previous button states (used for checking whether a change happened)
    int prev_button_state[BTN_COUNT] = {0, 0, 0, 0};
    // current button states
    int curr_button_state[BTN_COUNT] = {0, 0, 0, 0};
    // next time when the button should activate
    unsigned long button_next_repeat_millis[BTN_COUNT] = {0, 0, 0, 0};

    // when the last debounce happened in ms (used for debounce timer)
    unsigned long last_debounce_start = 0;

    unsigned long last_blink_start = 0;

    STORABLE_STATE_VARIABLES(int,;);

    bool approving_invalid_val = false;

    String lcd_top_line;
    String lcd_bottom_line;
};

struct MenuItem
{
    ClockVariable variable;
    String variable_str;

    int menu_value;
    int *value_in_state;

    String (*variable_val_str)(MenuItem *);

    void (*increment)(MenuItem *);
    void (*decrement)(MenuItem *);
    bool (*is_valid)(MenuItem *, State&);
};

struct Button
{
    ButtonColor color;
    int pin;

    bool should_debounce;
    bool allow_repeat;

    int (*get_repeat_offset_millis)(State&);
    void (*activate)(State&, MenuItem*, bool);
};

