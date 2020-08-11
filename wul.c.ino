#include <Wire.h>
#include <LiquidCrystal.h>
#include "RTClib.h"
#include <EEPROM.h>

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

const int buttons[] = {BTN_BLUE, BTN_RED, BTN_WHITE, BTN_GREEN};

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
    int last_debounce_start = millis();

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

State state;
EEPROMState eeprom_state;

RTC_DS3231 rtc;
LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

const int eeprom_addrs[] = {
    ENABLED_ADDR,
    START_MINUTES_ADDR,
    RAMP_UP_DURATION_MINUTES_ADDR,
    END_MINUTES_ADDR
};

int* eeprom_state_variables[] = {
    &eeprom_state.clock_enabled,
    &eeprom_state.start_time_minutes,
    &eeprom_state.ramp_up_duration_minutes,
    &eeprom_state.end_time_minutes
};

int eeprom_default_state_vals[] = { 
    1,     // enabled
    7 * 6, // start at 7AM (minutes/10)
    6,     // ramp for 1hr (minutes/10)
    9 * 6  // end at 9AM (minutes/10)
};

void turn_on_lcd();
void turn_off_lcd();

void turn_all_lights_on();
void turn_all_lights_off();

bool button_state_changed(int);
bool button_is(int, int);

void read_state_from_eeprom(State &state);
void write_state_to_eeprom(State &state);
void initialize_default_state_to_eeprom();
bool eeprom_is_initialized();

void setup () 
{
    Serial.begin(9600);

    if (!eeprom_is_initialized()) {
        initialize_default_state_to_eeprom();
    }

    read_state_from_eeprom();

    int outputs[] = {HIGH1, LOW2, LOW1, RELAY, RELAY2, LCD_BACKLIGHT};
    int lowOutputs[] = {HIGH1, LOW2, LOW1, RELAY, RELAY2};

    for (int i = 0; i < BTN_COUNT; i++) {
        pinMode(buttons[i], INPUT);
    }

    for (int i = 0; i < sizeof(outputs) / sizeof(int); i++) {
        pinMode(outputs[i], OUTPUT);
    }

    for (int i = 0; i < sizeof(lowOutputs) / sizeof(int); i++) {
        analogWrite(lowOutputs[i], 0);
    }

    lcd.begin(16,2);

    if (!rtc.begin()) {
        turn_on_lcd();
        lcd.write("RTC can't even!");

        while(1);
    }

    if (rtc.lostPower()) {
        turn_on_lcd();
        lcd.write("RTC lost power!");
        // todo: set time here

        while(1);
    }

    // Following line sets the RTC to the date & time this sketch was compiled
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void loop () 
{   
    DateTime now = rtc.now();

    const unsigned int day = now.day();
    const unsigned int hour = now.hour();
    const unsigned int minute = now.minute();
    const unsigned int ms = millis();

    const unsigned curr_mins = hour * 60 + minute;

    bool is_in_timeframe = curr_mins >= state.start_time_minutes 
        && curr_mins < state.end_time_minutes;

    if((ms - state.last_debounce_start) > DEBOUNCE_DELAY)
    {
        bool debounce = false;

        for (int i = 0; i < BTN_COUNT; i++) {
            state.curr_button_state[i] = digitalRead(buttons[i]);
        }

        if (is_in_timeframe && button_state_changed(Button::BLUE)) {
            if(button_is(Button::BLUE, HIGH)) {
                state.skip_day = state.skip_day == day ? -1 : day;
            }

            debounce = true;
        }

        if (button_is(Button::RED, HIGH) && button_state_changed(Button::RED)) {
            if (state.current_clock_state != ClockState::LIGHTS_ON) {
                state.prev_clock_state = state.current_clock_state;
                state.current_clock_state = ClockState::LIGHTS_ON;
            } else {
                state.current_clock_state = state.prev_clock_state;
            }

            debounce = true;
        }

        for (int i = 0; i < BTN_COUNT; i++) {
            state.prev_button_state[i] = state.curr_button_state[i];    
        }

        if (debounce) {
            state.last_debounce_start = ms;
        }
    }

    if (state.current_clock_state == ClockState::LIGHTS_ON) {
        turn_all_lights_on();
        return;
    }

    if (state.skip_day == day || !is_in_timeframe) {
        turn_all_lights_off();
        return;
    }

    // LED ramp-up
    {
        const int steps_between = state.ramp_up_duration_minutes;
        const int current_step = curr_mins - state.start_time_minutes;

        digitalWrite(RELAY, LOW);
        digitalWrite(RELAY2, LOW);

        const int step_interval = steps_between / 2;

        if (current_step < step_interval) {
            analogWrite(LOW1, (float)current_step / step_interval * 255);
        } else if (current_step >= steps_between) {
            turn_all_lights_on();
        } else if (current_step >= step_interval) {
            analogWrite(LOW1, 255);
            analogWrite(LOW2, (float)(current_step - step_interval) / step_interval * 255);
        }      
    }
}

void turn_all_lights_on()
{
    digitalWrite(RELAY, LOW);
    digitalWrite(RELAY2, LOW);

    analogWrite(LOW1,255);
    analogWrite(HIGH1, 255);
    analogWrite(LOW2, 255);
}

void turn_all_lights_off()
{
    digitalWrite(RELAY, HIGH);
    digitalWrite(RELAY2, HIGH);

    analogWrite(HIGH1, 0);
    analogWrite(LOW2, 0);
    analogWrite(LOW1, 0);
}

void turn_on_lcd()
{
    digitalWrite(LCD_BACKLIGHT, HIGH);
    lcd.display();
}

void turn_off_lcd()
{
    digitalWrite(LCD_BACKLIGHT, LOW);
    lcd.noDisplay();
}

bool button_state_changed(int btn) {
    return state.prev_button_state[btn] != state.curr_button_state[btn];
}

bool button_is(int btn, int val) {
    return state.curr_button_state[btn] == val;
}

void read_state_from_eeprom() 
{
    for (int i = 0; i < sizeof(eeprom_addrs) / sizeof(int); i++) {
        *eeprom_state_variables[i] = EEPROM.read(eeprom_addrs[i]);
    }

    state.start_time_minutes = eeprom_state.start_time_minutes * 10;
    state.ramp_up_duration_minutes = eeprom_state.ramp_up_duration_minutes * 10;
    state.end_time_minutes = eeprom_state.end_time_minutes * 10;
    state.current_clock_state = eeprom_state.clock_enabled ?
        ClockState::ACTIVE_TIMER : ClockState::DISABLED;
}

void write_state_to_eeprom()
{
    eeprom_state.start_time_minutes = state.start_time_minutes / 10;
    eeprom_state.ramp_up_duration_minutes = state.ramp_up_duration_minutes / 10;
    eeprom_state.end_time_minutes = state.end_time_minutes / 10;
    eeprom_state.clock_enabled = state.current_clock_state != ClockState::DISABLED;

    for (int i = 0; i < sizeof(eeprom_addrs) / sizeof(int); i++) {
        EEPROM.write(eeprom_addrs[i], *eeprom_state_variables[i]);
    }
}

void initialize_default_state_to_eeprom()
{
    for (int i = 0; i < sizeof(eeprom_addrs) / sizeof(int); i++) {
        EEPROM.write(eeprom_addrs[i], eeprom_default_state_vals[i]);
    }

    EEPROM.write(EEPROM.length() - 1, MAGIC_NUMBER);
}

bool eeprom_is_initialized()
{
    return EEPROM.read(EEPROM.length() - 1) == MAGIC_NUMBER;
}
