#include <Wire.h>
#include <LiquidCrystal.h>
#include "RTClib.h"
#include "definitions.h"
#include "formatting.h"
#include "eeprom.h"

void turn_on_lcd();
void turn_off_lcd();
void write_lcd(State &, String, String, bool = true);
void clear_lcd(State &);

void turn_all_lights_on();
void turn_all_lights_off(State &);

bool button_changed_to(int, int);

void reset_menu_item_vals();
MenuItem *current_menu_item(State);
ClockVariable next_variable(State);
ClockVariable prev_variable(State);

bool state_is(State, ClockState);

State global_state;
RTC_DS3231 rtc;
LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

MenuItem menu_items[] = {
    {
        ClockVariable::DISABLE_CLOCK,
        String("Disable clock?"),

        global_state.clock_is_disabled,
        &global_state.clock_is_disabled,

        [](MenuItem *i)
        {
            return String(i->menu_value ? "yes" : "no");
        },

        [](MenuItem *i)
        {
            i->menu_value = !i->menu_value;
        },
        [](MenuItem *i)
        {
            i->menu_value = !i->menu_value;
        }
    },
    {
        ClockVariable::START_TIME,
        String("Start time"),

        global_state.start_time_minutes,
        &global_state.start_time_minutes,

        [](MenuItem *i)
        {
            return minutes_to_time(i->menu_value);
        },

        [](MenuItem *i)
        {
            if (i->menu_value >= 24 * 60 - 10)
            {
                i->menu_value = 0;
            }
            else
            {
                i->menu_value = 10 + i->menu_value;
            }
        },
        [](MenuItem *i)
        {
            if (i->menu_value <= 0)
            {
                i->menu_value = 23 * 60 + 50;
            }
            else
            {
                i->menu_value = -10 + i->menu_value;
            }
        }
    },
    {
        ClockVariable::RAMP_UP_TIME,
        String("Ramp up time"),

        global_state.ramp_up_duration_minutes,
        &global_state.ramp_up_duration_minutes,

        [](MenuItem *i)
        {
            return minutes_to_duration(i->menu_value);
        },

        [](MenuItem *i)
        {
            if (i->menu_value >= 3 * 60)
            {
                i->menu_value = 0;
            }
            else
            {
                i->menu_value = 10 + i->menu_value;
            }
        },
        [](MenuItem *i)
        {
            if (i->menu_value <= 0)
            {
                i->menu_value = 3 * 60;
            }
            else
            {
                i->menu_value = -10 + i->menu_value;
            }
        }
    },
    {
        ClockVariable::END_TIME,
        String("End time"),

        global_state.end_time_minutes,
        &global_state.end_time_minutes,

        [](MenuItem *i)
        {
            return minutes_to_time(i->menu_value);
        },

        [](MenuItem *i)
        {
            if (i->menu_value >= 24 * 60 - 10)
            {
                i->menu_value = 0;
            }
            else
            {
                i->menu_value = 10 + i->menu_value;
            }
        },
        [](MenuItem *i)
        {
            if (i->menu_value <= 0)
            {
                i->menu_value = 23 * 60 + 50;
            }
            else
            {
                i->menu_value = -10 + i->menu_value;
            }
        }
    }
};

#define MENU_ITEM_COUNT sizeof(menu_items) / sizeof(MenuItem)

const int buttons[] = {BTN_BLUE, BTN_RED, BTN_WHITE, BTN_GREEN};

void setup()
{
    lcd.begin(16, 2);
    Serial.begin(9600);

    if (!eeprom_is_initialized())
    {
        initialize_default_state_to_eeprom();
    }

    read_state_from_eeprom(global_state);
    reset_menu_item_vals();

    int outputs[] = {HIGH1, LOW2, LOW1, RELAY, RELAY2, LCD_BACKLIGHT};
    int lowOutputs[] = {HIGH1, LOW2, LOW1, RELAY, RELAY2};

    for (int i = 0; i < BTN_COUNT; i++)
    {
        pinMode(buttons[i], INPUT);
    }

    for (int i = 0; i < sizeof(outputs) / sizeof(int); i++)
    {
        pinMode(outputs[i], OUTPUT);
    }

    for (int i = 0; i < sizeof(lowOutputs) / sizeof(int); i++)
    {
        analogWrite(lowOutputs[i], 0);
    }

    if (!rtc.begin())
    {
        turn_on_lcd();
        lcd.write("RTC can't even!");

        while (1);
    }

    if (rtc.lostPower())
    {
        turn_on_lcd();
        lcd.write("RTC lost power!");
        // todo: set time here

        while (1);
    }
}

void loop()
{
    auto menu_item = current_menu_item(global_state);

    DateTime now = rtc.now();
    const unsigned int ms = millis();
    const unsigned curr_mins = now.hour() * 60 + now.minute();

    bool is_in_timeframe =
        curr_mins >= global_state.start_time_minutes
        && curr_mins < global_state.end_time_minutes
        && !global_state.clock_is_disabled
        && global_state.skip_day != now.day();

    // read button states
    if ((ms - global_state.last_debounce_start) > DEBOUNCE_DELAY)
    {
        bool debounce = false;

        for (int i = 0; i < BTN_COUNT; i++)
        {
            global_state.curr_button_state[i] = digitalRead(buttons[i]);
        }

        if (button_changed_to(global_state, Button::BLUE, HIGH))
        {
            if (is_in_timeframe && state_is(global_state, ClockState::ACTIVE_TIMER))
            {
                global_state.skip_day = global_state.skip_day == now.day() ? -1 : now.day();
            }
            else if (state_is(global_state, ClockState::VARIABLE_SELECTION))
            {
                global_state.current_clock_variable = prev_variable(global_state);
            }
            else if (state_is(global_state, ClockState::CHANGING_VARIABLE))
            {
                menu_item->decrement(menu_item);
            }

            debounce = true;
        }

        if (button_changed_to(global_state, Button::RED, HIGH))
        {
            if (state_is(global_state, ClockState::VARIABLE_SELECTION))
            {
                global_state.current_clock_variable = next_variable(global_state);
            }
            else if (state_is(global_state, ClockState::CHANGING_VARIABLE))
            {
                menu_item->increment(menu_item);
            }
            else if (!state_is(global_state, ClockState::LIGHTS_ON))
            {
                global_state.current_clock_state = ClockState::LIGHTS_ON;
            }
            else
            {
                global_state.current_clock_state = ClockState::ACTIVE_TIMER;
            }

            debounce = true;
        }

        if (button_changed_to(global_state, Button::WHITE, HIGH))
        {
            if (state_is(global_state, ClockState::CHANGING_VARIABLE))
            {
                // save
                if (global_state.current_clock_variable == ClockVariable::DISABLE_CLOCK)
                {
                    global_state.clock_is_disabled = menu_item->menu_value;
                }
                if (global_state.current_clock_variable == ClockVariable::START_TIME)
                {
                    if (menu_item->menu_value > global_state.end_time_minutes)
                    {
                        write_lcd(global_state, "Start can't be", "after end time");
                        delay(3000);
                        return;
                    }
                    global_state.start_time_minutes = menu_item->menu_value;
                }
                if (global_state.current_clock_variable == ClockVariable::RAMP_UP_TIME)
                {
                    global_state.ramp_up_duration_minutes = menu_item->menu_value;
                }
                if (global_state.current_clock_variable == ClockVariable::END_TIME)
                {
                    if (menu_item->menu_value < global_state.start_time_minutes)
                    {
                        write_lcd(global_state, "End can't be", "before start");
                        delay(3000);
                        return;
                    }
                    global_state.end_time_minutes = menu_item->menu_value;
                }

                write_state_to_eeprom(global_state);
                global_state.current_clock_state = ClockState::VARIABLE_SELECTION;
            }
            else if (state_is(global_state, ClockState::VARIABLE_SELECTION))
            {
                global_state.current_clock_state = ClockState::CHANGING_VARIABLE;
            }
            else
            {
                global_state.current_clock_state = ClockState::VARIABLE_SELECTION;
                reset_menu_item_vals();
            }
        }

        if (button_changed_to(global_state, Button::GREEN, HIGH))
        {
            if (state_is(global_state, ClockState::CHANGING_VARIABLE))
            {
                global_state.current_clock_state = ClockState::VARIABLE_SELECTION;
                reset_menu_item_vals();
            }
            else if (state_is(global_state, ClockState::VARIABLE_SELECTION))
            {
                global_state.current_clock_state = ClockState::ACTIVE_TIMER;
                global_state.current_clock_variable = ClockVariable::DISABLE_CLOCK;

                clear_lcd(global_state);
                turn_off_lcd();
            }
            else
            {
                turn_on_lcd();
            }
        }
        else if (button_changed_to(global_state, Button::GREEN, LOW) && !in_menu(global_state))
        {
            turn_off_lcd();
        }

        for (int i = 0; i < BTN_COUNT; i++)
        {
            global_state.prev_button_state[i] = global_state.curr_button_state[i];
        }

        if (debounce)
        {
            global_state.last_debounce_start = ms;
        }
    }

    // draw menu / time
    if (in_menu(global_state))
    {
        String top_line = menu_item->variable_str,
            bottom_line = menu_item->variable_val_str(menu_item);

        if (state_is(global_state, ClockState::VARIABLE_SELECTION))
        {
            write_lcd(global_state, top_line, "> " + bottom_line);
        }
        else if (state_is(global_state, ClockState::CHANGING_VARIABLE))
        {
            // don't blink while pressing buttons
            if (ms - global_state.last_debounce_start > 1000)
            {
                if ((ms - global_state.last_blink_start) > 1500)
                {
                    global_state.last_blink_start = ms;
                }

                if (ms - global_state.last_blink_start < 750)
                {
                    bottom_line = "";
                }
            }

            write_lcd(global_state, top_line, "> " + bottom_line);
        }
    }
    else
    {
        write_lcd(
            global_state,
            "Now: " + left_pad(now.hour()) + ":" + left_pad(now.minute()) + ":" + left_pad(now.second()),
            "Alarm: " + minutes_to_time(global_state.start_time_minutes),
            false
        );
    }

    if (state_is(global_state, ClockState::LIGHTS_ON))
    {
        turn_all_lights_on();
    }
    else if (!is_in_timeframe)
    {
        // turn all lights off
        global_state.last_step = -1;
        digitalWrite(RELAY, HIGH);
        digitalWrite(RELAY2, HIGH);

        analogWrite(HIGH1, 0);
        analogWrite(LOW2, 0);
        analogWrite(LOW1, 0);
    }
    else
    {
        // LED ramp-up
        const int current_step = curr_mins - global_state.start_time_minutes;

        if (current_step == global_state.last_step)
        {
            return;
        }

        const int steps_between = global_state.ramp_up_duration_minutes;

        digitalWrite(RELAY, LOW);
        digitalWrite(RELAY2, LOW);

        const int step_interval = steps_between / 2;

        if (current_step < step_interval)
        {
            analogWrite(LOW1, (float) current_step / step_interval * 255);
        }
        else if (current_step >= steps_between)
        {
            turn_all_lights_on();
        }
        else if (current_step >= step_interval)
        {
            analogWrite(LOW1, 255);
            analogWrite(LOW2, (float) (current_step - step_interval) / step_interval * 255);
        }

        global_state.last_step = current_step;
    }
}

void turn_all_lights_on()
{
    digitalWrite(RELAY, LOW);
    digitalWrite(RELAY2, LOW);

    analogWrite(LOW1, 255);
    analogWrite(HIGH1, 255);
    analogWrite(LOW2, 255);
}

void turn_on_lcd()
{
    digitalWrite(LCD_BACKLIGHT, HIGH);
}

void turn_off_lcd()
{
    digitalWrite(LCD_BACKLIGHT, LOW);
}

void write_lcd(State &state, String top_line, String bottom_line, bool turn_on_display)
{
    if (turn_on_display)
    {
        turn_on_lcd();
    }

    if (state.lcd_top_line != top_line || state.lcd_bottom_line != bottom_line)
    {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(top_line);
        lcd.setCursor(0, 1);
        lcd.print(bottom_line);

        state.lcd_top_line = top_line;
        state.lcd_bottom_line = bottom_line;
    }
}

void clear_lcd(State &state)
{
    lcd.clear();
    state.lcd_top_line = "";
    state.lcd_bottom_line = "";
}

bool button_changed_to(State state, int btn, int val)
{
    return state.prev_button_state[btn] != state.curr_button_state[btn]
           && state.curr_button_state[btn] == val;
}

ClockVariable next_variable(State state)
{
    int current_index = (int) state.current_clock_variable;
    int result = current_index + 1;

    if (result == MENU_ITEM_COUNT)
    {
        result = 0;
    }

    return (ClockVariable) result;
}

ClockVariable prev_variable(State state)
{
    int current_index = (int) state.current_clock_variable;
    int result = current_index - 1;

    if (result < 0)
    {
        result = MENU_ITEM_COUNT - 1;
    }

    return (ClockVariable) result;
}

void reset_menu_item_vals()
{
    for (int i = 0; i < MENU_ITEM_COUNT; i++)
    {
        menu_items[i].menu_value = *menu_items[i].value_in_state;
    }
}

MenuItem *current_menu_item(State state)
{
    return &menu_items[(int) state.current_clock_variable];
}

bool state_is(State state, ClockState cs)
{
    return cs == state.current_clock_state;
}

bool in_menu(State state)
{
    return state_is(state, ClockState::CHANGING_VARIABLE)
           || state_is(state, ClockState::VARIABLE_SELECTION);
}
