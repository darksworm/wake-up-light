#include "definitions.h"

MenuItem* get_menu_items(State* state)
{
    static MenuItem items[] = {
        {
            ClockVariable::DISABLE_CLOCK,
            String("Disable clock?"),

            state->clock_is_disabled,
            &state->clock_is_disabled,

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

            state->start_time_minutes,
            &state->start_time_minutes,

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

            state->ramp_up_duration_minutes,
            &state->ramp_up_duration_minutes,

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

            state->end_time_minutes,
            &state->end_time_minutes,

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

            state->auto_adjust_duration_daily_minutes,
            &state->auto_adjust_duration_daily_minutes,

            [](MenuItem *i)
            {
                auto text = minutes_to_duration(i->menu_value);
                return text == "instant" ? "none" : text;
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

            state->target_start_time_minutes,
            &state->target_start_time_minutes,

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

    return items;
}
