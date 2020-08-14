#include <Wire.h>
#include <LiquidCrystal.h>
#include "RTClib.h"
#include "definitions.h"
#include "formatting.h"
#include "eeprom.h"

void turn_on_lcd();
void turn_off_lcd();
void write_lcd(String, String, bool = true);
void clear_lcd();

void turn_all_lights_on();
void turn_all_lights_off();

bool button_changed_to(int, int);

ClockVariable next_variable(State);
ClockVariable prev_variable(State);

State state;

RTC_DS3231 rtc;
LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

MenuItem menu_items[] = {
    {
        ClockVariable::DISABLE_CLOCK,
        String("Disable clock?"),

        state.clock_is_disabled,
        &state.clock_is_disabled,

        [](MenuItem* i) {
            return String(i->menu_value ? "yes" : "no");
        },

        [](MenuItem* i) {
            i->menu_value = !i->menu_value;
        },
        [](MenuItem* i) {
            i->menu_value = !i->menu_value;
        }
    },
    {
        ClockVariable::START_TIME,
        String("Start time"),

        state.start_time_minutes,
        &state.start_time_minutes,

        [] (MenuItem* i) {
            return minutes_to_time(i->menu_value);
        },

        [](MenuItem* i) {
            if (i->menu_value >= 24*60 - 10) {
                i->menu_value = 0;
            } else {
                i->menu_value = 10 + i->menu_value;
            }
        },
        [](MenuItem* i) {
            if (i->menu_value <= 0) {
                i->menu_value = 23*60 + 50;
            } else {
                i->menu_value = -10 + i->menu_value;
            }
        }
    },
    {
        ClockVariable::RAMP_UP_TIME,
        String("Ramp up time"),

        state.ramp_up_duration_minutes,
        &state.ramp_up_duration_minutes,

        [] (MenuItem* i) {
            return minutes_to_duration(i->menu_value);
        },

        [](MenuItem* i) {
            if(i->menu_value >= 3 * 60 - 10) {
                i->menu_value = 0;
            } else {
                i->menu_value = 10 + i->menu_value;
            }
        },
        [](MenuItem* i) {
            if (i->menu_value <= 0) {
                i->menu_value = 3 * 60;
            } else {
                i->menu_value = -10 + i->menu_value;
            }
        }
    },
    {
        ClockVariable::END_TIME,
        String("End time"),

        state.end_time_minutes,
        &state.end_time_minutes,

        [] (MenuItem* i) {
            return minutes_to_time(i->menu_value);
        },

        [](MenuItem* i) {
            if (i->menu_value >= 24*60 - 10) {
                i->menu_value = 0;
            } else {
                i->menu_value = 10 + i->menu_value;
            }
        },
        [](MenuItem* i) {
            if (i->menu_value <= 0) {
                i->menu_value = 23*60 + 50;
            } else {
                i->menu_value = -10 + i->menu_value;
            }
        }
    }
};

#define MENU_ITEM_COUNT sizeof(menu_items) / sizeof(MenuItem)

void reset_menu_item_vals()
{
    for (int i = 0; i < sizeof(menu_items) / sizeof(MenuItem); i++) {
        menu_items[i].menu_value = *menu_items[i].value_in_state;
    }
}

const int buttons[] = {BTN_BLUE, BTN_RED, BTN_WHITE, BTN_GREEN};

void setup () 
{
    lcd.begin(16,2);
    Serial.begin(9600);

    if (!eeprom_is_initialized()) {
        initialize_default_state_to_eeprom();
    }

    read_state_from_eeprom(state);
    reset_menu_item_vals();

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

int last_step = -1;

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

    if(state.current_clock_state != ClockState::VARIABLE_SELECTION 
    && state.current_clock_state != ClockState::CHANGING_VARIABLE) {
        write_lcd(
            "Now: " + left_pad(hour) + ":" + left_pad(minute) + ":" + left_pad(second),
            "Alarm: " + minutes_to_time(state.start_time_minutes),
            false
        );
    }

    auto menu_item = current_menu_item(state);

    if((ms - state.last_debounce_start) > DEBOUNCE_DELAY)
    {
        bool debounce = false;

        for (int i = 0; i < BTN_COUNT; i++) {
            state.curr_button_state[i] = digitalRead(buttons[i]);
        }

        if (button_changed_to(Button::BLUE, HIGH)) {
            if(is_in_timeframe && state.current_clock_state == ClockState::ACTIVE_TIMER) {
                state.skip_day = state.skip_day == day ? -1 : day;
            } else if (state.current_clock_state == ClockState::VARIABLE_SELECTION) {
                state.current_clock_variable = prev_variable(state);
            } else if (state.current_clock_state == ClockState::CHANGING_VARIABLE) {
                menu_item->decrement(menu_item);
            }

            debounce = true;
        }

        if (button_changed_to(Button::RED, HIGH)) {
            if(state.current_clock_state == ClockState::VARIABLE_SELECTION) {
                state.current_clock_variable = next_variable(state);
            } else if (state.current_clock_state == ClockState::CHANGING_VARIABLE) {
                menu_item->increment(menu_item);
            } else if (state.current_clock_state != ClockState::LIGHTS_ON) {
                state.current_clock_state = ClockState::LIGHTS_ON;
            } else {
                state.current_clock_state = ClockState::ACTIVE_TIMER;
            }

            debounce = true;
        }

        if (button_changed_to(Button::WHITE, HIGH)) {
            if(state.current_clock_state == ClockState::CHANGING_VARIABLE) {
                // save
                if (state.current_clock_variable == ClockVariable::DISABLE_CLOCK) {
                    state.clock_is_disabled = menu_item->menu_value;
                }
                if (state.current_clock_variable == ClockVariable::START_TIME) {
                    if(menu_item->menu_value > state.end_time_minutes) {
                        write_lcd("Start can't be", "after end time");
                        delay(3000);
                        return;
                    }
                    state.start_time_minutes = menu_item->menu_value;
                }
                if (state.current_clock_variable == ClockVariable::RAMP_UP_TIME) {
                    state.ramp_up_duration_minutes = menu_item->menu_value;
                }
                if (state.current_clock_variable == ClockVariable::END_TIME) {
                    if(menu_item->menu_value < state.start_time_minutes) {
                        write_lcd("End can't be", "before start");
                        delay(3000);
                        return;
                    }
                    state.end_time_minutes = menu_item->menu_value;
                }

                write_state_to_eeprom(state);
                state.current_clock_state = ClockState::VARIABLE_SELECTION;
            } else if(state.current_clock_state == ClockState::VARIABLE_SELECTION) {
                state.current_clock_state = ClockState::CHANGING_VARIABLE;
            } else {
                state.current_clock_state = ClockState::VARIABLE_SELECTION;
                reset_menu_item_vals();
            }
        }

        if (button_changed_to(Button::GREEN, HIGH)) {
            if (state.current_clock_state == ClockState::CHANGING_VARIABLE) {
                state.current_clock_state = ClockState::VARIABLE_SELECTION;
                reset_menu_item_vals();
            } else if (state.current_clock_state == ClockState::VARIABLE_SELECTION) {
                state.current_clock_state = ClockState::ACTIVE_TIMER;
                clear_lcd();
                turn_off_lcd();
            } else {
                turn_on_lcd();
            }
        } else if (button_changed_to(Button::GREEN, LOW) && state.current_clock_state != ClockState::VARIABLE_SELECTION && state.current_clock_state != ClockState::CHANGING_VARIABLE) {
            turn_off_lcd();
        }

        for (int i = 0; i < BTN_COUNT; i++) {
            state.prev_button_state[i] = state.curr_button_state[i];    
        }

        if (debounce) {
            state.last_debounce_start = ms;
        }
    }

    String top_line = menu_item->variable_str,
           bottom_line = menu_item->variable_val_str(menu_item);

    switch (state.current_clock_state) {
        case ClockState::LIGHTS_ON:
            turn_all_lights_on();
            return;
        case ClockState::VARIABLE_SELECTION:

            write_lcd(top_line, "> " + bottom_line);
            break;
        case ClockState::CHANGING_VARIABLE:
            // don't blink while pressing buttons
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

    if (state.skip_day == day || !is_in_timeframe || state.clock_is_disabled) {
        turn_all_lights_off();
        return;
    }

    // LED ramp-up
    {
        const int current_step = curr_mins - state.start_time_minutes;

        if(current_step == last_step) {
            return;
        }

        const int steps_between = state.ramp_up_duration_minutes;

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

        last_step = current_step;
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
    last_step = -1;
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

bool button_changed_to(int btn, int val) {
    return state.prev_button_state[btn] != state.curr_button_state[btn]
        && state.curr_button_state[btn] == val;
}

ClockVariable next_variable(State state) 
{
    int current_index = (int)state.current_clock_variable;
    int result = current_index + 1;

    if (result == MENU_ITEM_COUNT) {
        result = 0;
    }

    return (ClockVariable) result;
}

ClockVariable prev_variable(State state)
{
    int current_index = (int)state.current_clock_variable;
    int result = current_index - 1;

    if (result < 0) {
        result = MENU_ITEM_COUNT - 1;
    }

    return (ClockVariable) result;
}

MenuItem* current_menu_item(State state) 
{
    return &menu_items[(int)state.current_clock_variable];
}
