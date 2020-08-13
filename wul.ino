#include <Wire.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include "RTClib.h"
#include "definitions.h"

void turn_on_lcd();
void turn_off_lcd();
void write_lcd(String, String, bool = true);
void clear_lcd();

void turn_all_lights_on();
void turn_all_lights_off();

bool button_state_changed(int);
bool button_is(int, int);

void read_state_from_eeprom(State &state);
void write_state_to_eeprom(State &state);
void initialize_default_state_to_eeprom();
bool eeprom_is_initialized();
String current_variable_str();
String current_variable_val_str();
ClockVariable next_variable();
ClockVariable prev_variable();

String minutes_to_time();
String left_pad(int);

State state;
EEPROMState eeprom_state;
State temp_menu_state;

RTC_DS3231 rtc;
LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

const int buttons[] = {BTN_BLUE, BTN_RED, BTN_WHITE, BTN_GREEN};
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

static_assert(sizeof(eeprom_addrs) / sizeof(const int) == 
        sizeof(eeprom_state_variables) / sizeof(int*));

static_assert(sizeof(eeprom_default_state_vals) / sizeof(int) ==
        sizeof(eeprom_state_variables) / sizeof(int*));


void setup () 
{
    lcd.begin(16,2);
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
}

void loop () 
{   
    DateTime now = rtc.now();

    const unsigned int day = now.day();
    const unsigned int hour = now.hour();
    const unsigned int minute = now.minute();
    const unsigned int second = now.second();
    const unsigned int ms = millis();

    const unsigned curr_mins = hour * 60 + minute;

    bool is_in_timeframe = curr_mins >= state.start_time_minutes 
        && curr_mins < state.end_time_minutes;

    if(state.current_clock_state != ClockState::VARIABLE_SELECTION && state.current_clock_state != ClockState::CHANGING_VARIABLE) {
        write_lcd(
            "Now: " + left_pad(hour) + ":" + left_pad(minute) + ":" + left_pad(second),
            "Alarm: " + minutes_to_time(state.start_time_minutes),
            false
        );
    }

    if((ms - state.last_debounce_start) > DEBOUNCE_DELAY)
    {
        bool debounce = false;

        for (int i = 0; i < BTN_COUNT; i++) {
            state.curr_button_state[i] = digitalRead(buttons[i]);
        }

        if (button_is(Button::BLUE, HIGH) && button_state_changed(Button::BLUE)) {
            if(is_in_timeframe && state.current_clock_state == ClockState::ACTIVE_TIMER) {
                state.skip_day = state.skip_day == day ? -1 : day;
            } else if (state.current_clock_state == ClockState::VARIABLE_SELECTION) {
                state.current_clock_variable = prev_variable();
            } else if (state.current_clock_state == ClockState::CHANGING_VARIABLE) {
                // dec variable
                if (state.current_clock_variable == ClockVariable::DISABLE_CLOCK) {
                    temp_menu_state.clock_is_disabled = !temp_menu_state.clock_is_disabled;
                }
                if (state.current_clock_variable == ClockVariable::START_TIME) {
                    if (temp_menu_state.start_time_minutes <= 0) {
                        temp_menu_state.start_time_minutes = 23*60 + 50;
                    } else {
                        temp_menu_state.start_time_minutes = -10 + temp_menu_state.start_time_minutes;
                    }
                }
                if (state.current_clock_variable == ClockVariable::RAMP_UP_TIME) {
                    if (temp_menu_state.ramp_up_duration_minutes <= 0) {
                        temp_menu_state.ramp_up_duration_minutes = 3 * 60;
                    } else {
                        temp_menu_state.ramp_up_duration_minutes = -10 + temp_menu_state.ramp_up_duration_minutes;
                    }
                }
                if (state.current_clock_variable ==  ClockVariable::END_TIME) {
                    if (temp_menu_state.end_time_minutes <= 0) {
                        temp_menu_state.end_time_minutes = 23*60 + 50;
                    } else {
                        temp_menu_state.end_time_minutes = -10 + temp_menu_state.end_time_minutes;
                    }
                }
            }

            debounce = true;
        }

        if (button_is(Button::RED, HIGH) && button_state_changed(Button::RED)) {
            if(state.current_clock_state == ClockState::VARIABLE_SELECTION) {
                state.current_clock_variable = next_variable();
            } else if (state.current_clock_state == ClockState::CHANGING_VARIABLE) {
                // inc variable
                if (state.current_clock_variable == ClockVariable::DISABLE_CLOCK) {
                    temp_menu_state.clock_is_disabled = !temp_menu_state.clock_is_disabled;
                }
                if (state.current_clock_variable == ClockVariable::START_TIME) {
                    if (temp_menu_state.start_time_minutes >= 24*60 - 10) {
                        temp_menu_state.start_time_minutes = 0;
                    } else {
                        temp_menu_state.start_time_minutes = 10 + temp_menu_state.start_time_minutes;
                    }
                }
                if (state.current_clock_variable == ClockVariable::RAMP_UP_TIME) {
                    if(temp_menu_state.ramp_up_duration_minutes >= 3 * 60 - 10) {
                        temp_menu_state.ramp_up_duration_minutes = 0;
                    } else {
                        temp_menu_state.ramp_up_duration_minutes = 10 + temp_menu_state.ramp_up_duration_minutes;
                    }
                }
                if (state.current_clock_variable ==  ClockVariable::END_TIME) {
                    if (temp_menu_state.end_time_minutes >= 24*60 - 10) {
                        temp_menu_state.end_time_minutes = 0;
                    } else {
                        temp_menu_state.end_time_minutes = 10 + temp_menu_state.end_time_minutes;
                    }
                }
            } else if (state.current_clock_state != ClockState::LIGHTS_ON) {
                state.current_clock_state = ClockState::LIGHTS_ON;
            } else {
                state.current_clock_state =
                    state.clock_is_disabled ? ClockState::DISABLED 
                    : ClockState::ACTIVE_TIMER;
            }

            debounce = true;
        }

        if (button_is(Button::WHITE, HIGH) && button_state_changed(Button::WHITE)) {
            if(state.current_clock_state == ClockState::CHANGING_VARIABLE) {
                // save
                if (state.current_clock_variable == ClockVariable::DISABLE_CLOCK) {
                    state.clock_is_disabled = temp_menu_state.clock_is_disabled;
                }
                if (state.current_clock_variable == ClockVariable::START_TIME) {
                    state.start_time_minutes = temp_menu_state.start_time_minutes;
                }
                if (state.current_clock_variable == ClockVariable::RAMP_UP_TIME) {
                    state.ramp_up_duration_minutes = temp_menu_state.ramp_up_duration_minutes;
                }
                if (state.current_clock_variable == ClockVariable::END_TIME) {
                    state.end_time_minutes = temp_menu_state.end_time_minutes;
                }

                write_state_to_eeprom();
                state.current_clock_state = ClockState::VARIABLE_SELECTION;
            } else if(state.current_clock_state == ClockState::VARIABLE_SELECTION) {
                state.current_clock_state = ClockState::CHANGING_VARIABLE;
            } else {
                state.current_clock_state = ClockState::VARIABLE_SELECTION;
                temp_menu_state = state;
            }
        }

        if (button_is(Button::GREEN, HIGH) && button_state_changed(Button::GREEN)) {
            if (state.current_clock_state == ClockState::CHANGING_VARIABLE) {
                state.current_clock_state = ClockState::VARIABLE_SELECTION;
            } else if (state.current_clock_state == ClockState::VARIABLE_SELECTION) {
                state.current_clock_state =
                    state.clock_is_disabled ? ClockState::DISABLED 
                    : ClockState::ACTIVE_TIMER;

                clear_lcd();
                turn_off_lcd();
            } else {
                turn_on_lcd();
            }
        } else if (button_is(Button::GREEN, LOW) && button_state_changed(Button::GREEN) && state.current_clock_state != ClockState::VARIABLE_SELECTION && state.current_clock_state != ClockState::CHANGING_VARIABLE) {
            turn_off_lcd();
        }

        for (int i = 0; i < BTN_COUNT; i++) {
            state.prev_button_state[i] = state.curr_button_state[i];    
        }

        if (debounce) {
            state.last_debounce_start = ms;
        }
    }

    String top_line, bottom_line;
    switch (state.current_clock_state) {
        case ClockState::LIGHTS_ON:
            turn_all_lights_on();
            return;
        case ClockState::VARIABLE_SELECTION:
            top_line = current_variable_str();
            bottom_line = current_variable_val_str();

            write_lcd(top_line, "> " + bottom_line);
            break;
        case ClockState::CHANGING_VARIABLE:
            top_line = current_variable_str();
            bottom_line = current_variable_val_str();

            // don't blink while im pressing buttons
            if (ms - state.last_debounce_start > 1000) {
                if ((ms - state.last_blink_start) > 1500) {
                    state.last_blink_start = ms;
                } 

                if (ms - state.last_blink_start < 750) {
                    bottom_line = "";
                }
            }

            write_lcd(top_line, "> " + bottom_line);
            break;
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
}

void turn_off_lcd()
{
    digitalWrite(LCD_BACKLIGHT, LOW);
}

void write_lcd(String top_line, String bottom_line, bool turn_on_display) 
{
    if(turn_on_display) {
        turn_on_lcd();
    }

    if (state.lcd_top_line != top_line 
            || state.lcd_bottom_line != bottom_line) {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(top_line);
        lcd.setCursor(0,1);
        lcd.print(bottom_line);

        state.lcd_top_line = top_line;
        state.lcd_bottom_line = bottom_line;
    }
}

void clear_lcd()
{
    lcd.clear();
    state.lcd_top_line = "";
    state.lcd_bottom_line = "";
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
    state.clock_is_disabled = !eeprom_state.clock_enabled;
    state.current_clock_state = eeprom_state.clock_enabled ?
        ClockState::ACTIVE_TIMER : ClockState::DISABLED;
}

void write_state_to_eeprom()
{
    eeprom_state.start_time_minutes = state.start_time_minutes / 10;
    eeprom_state.ramp_up_duration_minutes = state.ramp_up_duration_minutes / 10;
    eeprom_state.end_time_minutes = state.end_time_minutes / 10;
    eeprom_state.clock_enabled = !state.clock_is_disabled;

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

String current_variable_str()
{
    if (state.current_clock_variable == ClockVariable::DISABLE_CLOCK) {
        return "Disable clock?";
    }
    if (state.current_clock_variable == ClockVariable::START_TIME) {
        return "Start time";
    }
    if (state.current_clock_variable == ClockVariable::RAMP_UP_TIME) {
        return "Ramp up time";
    }
    if (state.current_clock_variable == ClockVariable::END_TIME) {
        return "End time";
    }

    return "unimplemented";
}

String minutes_to_time(int mins)
{
    int hrs = mins / 60;
    int leftover_mins = mins - hrs * 60;

    String result = String(hrs) + ":" + String(leftover_mins);

    if (leftover_mins < 10) {
        result += "0";
    }

    if (hrs < 10) {
        result = "0" + result;
    }
    
    return result;
}

String minutes_to_duration(int mins)
{
    if (mins == 0) {
        return "instant";
    }

    String result = "";

    int hrs = mins / 60;
    int leftover_mins = mins - hrs * 60;

    if (hrs > 1) {
        result += String(hrs) + "hrs";
    } else if (hrs != 0) {
        result += String(hrs) + "hr";
    }

    if (hrs && leftover_mins) {
        result += " ";
    }

    if (leftover_mins > 0) {
        result += String(leftover_mins) + " mins";
    }
    
    return result;
}

String current_variable_val_str()
{
    if (state.current_clock_variable == ClockVariable::DISABLE_CLOCK) {
        return temp_menu_state.clock_is_disabled ? "yes" : "no";
    }
    if (state.current_clock_variable == ClockVariable::START_TIME) {
        return minutes_to_time(temp_menu_state.start_time_minutes);
    }
    if (state.current_clock_variable == ClockVariable::RAMP_UP_TIME) {
        return minutes_to_duration(temp_menu_state.ramp_up_duration_minutes);
    }
    if (state.current_clock_variable == ClockVariable::END_TIME) {
        return minutes_to_time(temp_menu_state.end_time_minutes);
    }

    return "unimplemented";
}

ClockVariable next_variable() 
{
    if (state.current_clock_variable == ClockVariable::DISABLE_CLOCK) {
        return ClockVariable::START_TIME;
    }
    if (state.current_clock_variable == ClockVariable::START_TIME) {
        return ClockVariable::RAMP_UP_TIME;
    }
    if (state.current_clock_variable == ClockVariable::RAMP_UP_TIME) {
        return ClockVariable::END_TIME;
    }

    return ClockVariable::DISABLE_CLOCK;
}

ClockVariable prev_variable()
{
    if (state.current_clock_variable == ClockVariable::START_TIME) {
        return ClockVariable::DISABLE_CLOCK;
    }
    if (state.current_clock_variable == ClockVariable::RAMP_UP_TIME) {
        return ClockVariable::START_TIME;
    }
    if (state.current_clock_variable == ClockVariable::END_TIME) {
        return ClockVariable::RAMP_UP_TIME;
    }

    return ClockVariable::END_TIME;
}

String left_pad(int v) {
    if(v < 10) {
        return "0" + String(v);
    }

    return String(v);
}
