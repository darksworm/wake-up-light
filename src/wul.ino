#include <Wire.h>
#include <LiquidCrystal_I2C.h>
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

bool button_changed_to(const State&, int, int);
bool button_ready_to_repeat(const State&, int, unsigned long);

void reset_menu_item_vals();
MenuItem *current_menu_item(const State&);
ClockVariable next_variable(const State&);
ClockVariable prev_variable(const State&);

bool state_is(const State&, ClockState);
void read_button_pins(State&);

State global_state;
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 20, 4);

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
        },
        [](MenuItem *i, State& s)
        {
            return true;
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
        },
        [](MenuItem *i, State& s)
        {
            return i->menu_value < s.end_time_minutes &&
                s.end_time_minutes - i->menu_value >= s.ramp_up_duration_minutes;
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
        },
        [](MenuItem *i, State& s)
        {
            return s.end_time_minutes - s.start_time_minutes >= i->menu_value;
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
        },
        [](MenuItem *i, State& s)
        {
            return i->menu_value > s.start_time_minutes &&
                 i->menu_value - s.start_time_minutes >= s.ramp_up_duration_minutes;
        }
    },
    {
        ClockVariable::AUTO_ADJUST_DURATION,
        String("Daily adjustment"),

        global_state.auto_adjust_duration_daily_minutes,
        &global_state.auto_adjust_duration_daily_minutes,

        [](MenuItem *i)
        {
            auto text = minutes_to_duration(i->menu_value);
            return text == "instant" ? "OFF" : text;
        },

        [](MenuItem *i)
        {
            if (i->menu_value >= 20)
            {
                i->menu_value = 0;
            }
            else
            {
                i->menu_value = 1 + i->menu_value;
            }
        },
        [](MenuItem *i)
        {
            if (i->menu_value <= 0)
            {
                i->menu_value = 20;
            }
            else
            {
                i->menu_value = -1 + i->menu_value;
            }
        },
        [](MenuItem *i, State& s)
        {
            return true;
        }
    },
    {
        ClockVariable::TARGET_START_TIME,
        String("Target start"),

        global_state.target_start_time_minutes,
        &global_state.target_start_time_minutes,

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
        },
        [](MenuItem *i, State& s)
        {
            return true;
        }
    }
};

#define MENU_ITEM_COUNT sizeof(menu_items) / sizeof(MenuItem)

const Button buttons[] = {
    {
        ButtonColor::BLUE,
        BTN_BLUE,
        true,
        true,
        [](State& s)
        {
            return state_is(s, ClockState::VARIABLE_SELECTION) ? 600 : 200;
        },
        [](State& s, MenuItem* i, bool is_in_timeframe)
        {
            if (is_in_timeframe && state_is(s, ClockState::ACTIVE_TIMER))
            {
                DateTime now = rtc.now();
                s.skip_day = s.skip_day == now.day() ? -1 : now.day();
            }
            else if (state_is(s, ClockState::VARIABLE_SELECTION))
            {
                s.current_clock_variable = prev_variable(s);
            }
            else if (state_is(s, ClockState::CHANGING_VARIABLE))
            {
                i->decrement(i);
            }
        }
    },
    {
        ButtonColor::RED,
        BTN_RED,
        true,
        true,
        [](State& s)
        {
            return state_is(s, ClockState::VARIABLE_SELECTION) ? 600 : 200;
        },
        [](State& s, MenuItem* i, bool is_in_timeframe)
        {
            if (state_is(s, ClockState::VARIABLE_SELECTION))
            {
                s.current_clock_variable = next_variable(s);
            }
            else if (state_is(s, ClockState::CHANGING_VARIABLE))
            {
                i->increment(i);
            }
            else if (!state_is(s, ClockState::LIGHTS_ON))
            {
                s.current_clock_state = ClockState::LIGHTS_ON;
            }
            else
            {
                s.current_clock_state = ClockState::ACTIVE_TIMER;
            }
        }
    },
    {
        ButtonColor::WHITE,
        BTN_WHITE,
        false,
        false,
        [](State& s)
        {
            return 0;
        },
        [](State& s, MenuItem* i, bool is_in_timeframe)
        {
            if (state_is(s, ClockState::CHANGING_VARIABLE))
            {
                bool is_valid = i->is_valid(i, s);

                if (is_valid || s.approving_invalid_val)
                {
                    *i->value_in_state = i->menu_value;
                    write_state_to_eeprom(s);

                    s.current_clock_state = ClockState::VARIABLE_SELECTION;
                    s.approving_invalid_val = false;
                }
                else
                {
                    s.approving_invalid_val = true;
                }
            }
            else if (state_is(s, ClockState::VARIABLE_SELECTION))
            {
                s.current_clock_state = ClockState::CHANGING_VARIABLE;
            }
            else
            {
                s.current_clock_state = ClockState::VARIABLE_SELECTION;
                reset_menu_item_vals();
            }
        }
    },
    {
        ButtonColor::GREEN,
        BTN_GREEN,
        true,
        false,
        [](State& s)
        {
            return 0;
        },
        [](State& s, MenuItem* i, bool is_in_timeframe)
        {
            if (state_is(s, ClockState::CHANGING_VARIABLE))
            {
                if (!s.approving_invalid_val)
                {
                    s.current_clock_state = ClockState::VARIABLE_SELECTION;
                }

                s.approving_invalid_val = false;
                reset_menu_item_vals();
            }
            else if (state_is(s, ClockState::VARIABLE_SELECTION))
            {
                s.current_clock_state = ClockState::ACTIVE_TIMER;
                s.current_clock_variable = ClockVariable::DISABLE_CLOCK;

                clear_lcd(s);
                turn_off_lcd();
            }
            else
            {
                if (digitalRead(LCD_BACKLIGHT))
                {
                    turn_off_lcd();
                }
                else
                {
                    turn_on_lcd();
                }
            }
        }
    },
};

static_assert(sizeof(buttons) / sizeof(Button) == BTN_COUNT);

void setup()
{
    lcd.begin(16, 2);

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
        pinMode(button[i].pin, INPUT);
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
        lcd.print("RTC can't even!");

        while (1);
    }

    if (rtc.lostPower())
    {
        turn_on_lcd();
        lcd.print("RTC lost power!");
        // todo: set time here

        while (1);
    }
}

void loop()
{
    auto menu_item = current_menu_item(global_state);

    DateTime now = rtc.now();
    const unsigned long ms = millis();
    const unsigned curr_mins = now.hour() * 60 + now.minute();

    // avoid millis overflow issues by resetting millis based state variables
    // this won't fire on the first run due to setup taking ~1000ms
    if (ms <= 100)
    {
        for(int i = 0; i < BTN_COUNT; i++)
        {
            global_state.button_next_repeat_millis[i] = 0;
        }

        global_state.last_debounce_start = 0;
        global_state.last_blink_start = 0;
    }

    bool is_in_timeframe =
        curr_mins >= global_state.start_time_minutes
        && curr_mins < global_state.end_time_minutes
        && !global_state.clock_is_disabled
        && global_state.skip_day != now.day();

    // read button states
    if ((ms - global_state.last_debounce_start) > DEBOUNCE_DELAY)
    {
        bool debounce = false;
        read_button_pins(global_state);

        for (int i = 0; i < BTN_COUNT; i++)
        {
            Button button = buttons[i];
            bool btn_changed = button_changed_to(global_state, (int)button.color, HIGH);

            if (button.allow_repeat)
            {
                bool btn_should_activate = btn_changed
                    || (button.allow_repeat && button_ready_to_repeat(global_state, (int)button.color, ms));

                if (global_state.curr_button_state[(int)button.color] == HIGH)
                {
                    if (btn_changed)
                    {
                        global_state.button_next_repeat_millis[i] = ms + 900;
                    }
                    else if (btn_should_activate)
                    {
                        global_state.button_next_repeat_millis[i] = ms + button.get_repeat_offset_millis(global_state);
                        debounce = true;
                    }
                }
                else if (button_changed_to(global_state, (int)button.color, LOW))
                {
                    global_state.button_next_repeat_millis[i] = 0;
                }
            }

            if (btn_should_activate)
            {
                button.activate(global_state, menu_item, is_in_timeframe);

                if (button.should_debounce)
                {
                    debounce = true;
                }
            }
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
            bottom_line = menu_item->variable_val_str(menu_item),
            pos_line = "";

        {
            int menu_pos = (int)global_state.current_clock_variable + 1;
            int hidden_items = global_state.auto_adjust_duration_daily_minutes == 0;

            int total_items = MENU_ITEM_COUNT - hidden_items;
            if (menu_pos > total_items) {
                menu_pos = total_items;
            }
            String pos_indicator = String(menu_pos) + "/" + String(total_items);

            int padding_size = 16 - 1 - bottom_line.length() - pos_indicator.length();

            for (int i = 1; i < padding_size; i++)
            {
                pos_line += " ";
            }

            pos_line += pos_indicator;
        }

        if (state_is(global_state, ClockState::VARIABLE_SELECTION))
        {
            write_lcd(global_state, top_line, "> " + bottom_line + pos_line);
        }
        else if (state_is(global_state, ClockState::CHANGING_VARIABLE))
        {
            if (global_state.approving_invalid_val)
            {
                write_lcd(global_state, "Invalid value!", "Save anyway?");
            }
            else
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
    }
    else
    {
        String top_text = "Now: " + left_pad(now.hour()) + ":"
            + left_pad(now.minute()) + ":" + left_pad(now.second());

        String bottom_text = "";

        for (int i = 0; i < MENU_ITEM_COUNT; i++)
        {
            if (!menu_items[i].is_valid(&menu_items[i], global_state))
            {
                bottom_text = "Bad settings!";
                break;
            }
        }

        if (bottom_text == "")
        {
            bottom_text = global_state.clock_is_disabled ?
                "Alarm disabled" :
                "Alarm: " + minutes_to_time(global_state.start_time_minutes);
        }

        write_lcd(
            global_state,
            top_text,
            bottom_text,
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
        bool should_adjust_time =
            global_state.last_adjustment_day != now.day() &&
            global_state.auto_adjust_duration_daily_minutes > 0 &&
            global_state.target_start_time_minutes != global_state.start_time_minutes;

        if (should_adjust_time)
        {
            int multiplier =
                global_state.target_start_time_minutes > global_state.start_time_minutes ? 1 : -1;

            int adjustment = multiplier * global_state.auto_adjust_duration_daily_minutes;
            {
                int adjusted_start = global_state.start_time_minutes + adjustment;

                if (1 == multiplier && adjusted_start > global_state.target_start_time_minutes)
                {
                    adjustment = global_state.target_start_time_minutes - global_state.start_time_minutes;
                }
                else if (-1 == multiplier && adjusted_start < global_state.target_start_time_minutes)
                {
                    adjustment = global_state.target_start_time_minutes - global_state.start_time_minutes;
                }
            }

            global_state.start_time_minutes += adjustment;
            global_state.end_time_minutes += adjustment;
            global_state.last_adjustment_day = now.day();

            if (global_state.start_time_minutes == global_state.target_start_time_minutes)
            {
                // adjustment has finished, disable it
                global_state.auto_adjust_duration_daily_minutes = 0;
            }

            write_state_to_eeprom(global_state);

            return;
        }

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
    lcd.backlight();
    digitalWrite(LCD_BACKLIGHT, HIGH);
}

void turn_off_lcd()
{
    lcd.noBacklight();
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

bool button_changed_to(const State& state, int btn, int val)
{
    return state.prev_button_state[btn] != state.curr_button_state[btn]
           && state.curr_button_state[btn] == val;
}

bool button_ready_to_repeat(const State& state, int btn, unsigned long ms_now)
{
    return state.curr_button_state[btn]
        && state.button_next_repeat_millis[btn]
        && state.button_next_repeat_millis[btn] <= ms_now;
}

ClockVariable next_variable(const State& state)
{
    int current_index = (int) state.current_clock_variable;
    int result = current_index + 1;

    // skip target start time if auto adjust set to zero
    if (state.auto_adjust_duration_daily_minutes == 0
            && result == (int)ClockVariable::TARGET_START_TIME)
    {
        result += 1;
    }

    if (result >= MENU_ITEM_COUNT)
    {
        result = 0;
    }

    return (ClockVariable) result;
}

ClockVariable prev_variable(const State& state)
{
    int current_index = (int) state.current_clock_variable;
    int result = current_index - 1;

    if (result < 0)
    {
        result = MENU_ITEM_COUNT - 1;
    }

    // NOTE: this will break if TARGET_START_TIME is put in the 0th position
    // skip target start time if auto adjust set to zero
    if (state.auto_adjust_duration_daily_minutes == 0
            && result == (int)ClockVariable::TARGET_START_TIME)
    {
        result -= 1;
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

MenuItem *current_menu_item(const State& state)
{
    return &menu_items[(int) state.current_clock_variable];
}

bool state_is(const State& state, ClockState cs)
{
    return cs == state.current_clock_state;
}

bool in_menu(State state)
{
    return state_is(state, ClockState::CHANGING_VARIABLE)
           || state_is(state, ClockState::VARIABLE_SELECTION);
}

void read_button_pins(State& global_state)
{
    for (int i = 0; i < BTN_COUNT; i++)
    {
        global_state.curr_button_state[i] = digitalRead(buttons[i].pin);
    }
}

