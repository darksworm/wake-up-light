#include <Wire.h>
#include <LiquidCrystal.h>
#include "RTClib.h"
#include <EEPROM.h>

#define LCD_ENABLE A0
#define LCD_D4 A1
#define LCD_D5 A2
#define LCD_D6 A3
#define LCD_D7 A4
#define LCD_RS A5
#define LCD_BACKLIGHT 7

#define BTN_BLUE 11
#define BTN_RED 12
#define BTN_WHITE 8
#define BTN_GREEN 13
#define BTN_COUNT 4

#define RELAY2 9
#define RELAY 10
#define HIGH1 4
#define LOW2 6
#define LOW1 5

#define HOUR_START 7
#define HOUR_FULL 8
#define HOUR_END 9
#define DEBOUNCE_DELAY 200

void turn_on_lcd();
void turn_off_lcd();

bool button_state_changed(int);
bool button_is(int, int);

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
    ClockState current_clock_state = ClockState::ACTIVE_TIMER;
    ClockVariable current_clock_variable = ClockVariable::DISABLE_CLOCK;

    int prev_button_state[BTN_COUNT] = {0, 0, 0, 0};
    int curr_button_state[BTN_COUNT] = {0, 0, 0, 0};
    int last_debounce_start = millis();
};

const int buttons[] = {BTN_BLUE, BTN_RED, BTN_WHITE, BTN_GREEN};

int skip_day = -1;
bool turn_lights_on = false;

const int steps_between = HOUR_FULL - HOUR_START + 1 * 60;

RTC_DS3231 rtc;
LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
State state;

void setup () 
{
    delay(700);
    Serial.begin(9600);
    delay(700);

    pinMode(HIGH1, OUTPUT);
    pinMode(LOW2, OUTPUT);
    pinMode(LOW1, OUTPUT);
    pinMode(RELAY, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    pinMode(LCD_BACKLIGHT, OUTPUT);

    for (int i = 0; i < BTN_COUNT; i++) {
        pinMode(buttons[i], INPUT);
    }

    analogWrite(HIGH1, 0);
    analogWrite(LOW2, 0);
    analogWrite(LOW1, 0);

    analogWrite(RELAY, 0);
    analogWrite(RELAY2, 0);

    lcd.begin(16,2);

    if (!rtc.begin()) {
        while(1) {
            analogWrite(LED_BUILTIN, 255);
            delay(750);
            analogWrite(LED_BUILTIN, 128);
            delay(750);
            analogWrite(LED_BUILTIN, 0);
            delay(750);
        }
    }

    if (rtc.lostPower()) {
        while(1) {
            analogWrite(LED_BUILTIN, 0);
            delay(750);
            analogWrite(LED_BUILTIN, 64);
            delay(750);
            analogWrite(LED_BUILTIN, 128);
            delay(750);
        }
    }

    // Following line sets the RTC to the date & time this sketch was compiled
   // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    
    turn_on_lcd();
    lcd.write("hello, world!");
}

void loop () 
{   
    DateTime now = rtc.now();

    const unsigned int day = now.day();
    const unsigned int hour = now.hour();
    const unsigned int minute = now.minute();
    const unsigned int ms = millis();

    bool is_in_timeframe = hour >= HOUR_START && hour < HOUR_END;

    if((ms - state.last_debounce_start) > DEBOUNCE_DELAY)
    {
        bool debounce = false;

        for (int i = 0; i < BTN_COUNT; i++) {
            state.curr_button_state[i] = digitalRead(buttons[i]);
        }

        if (is_in_timeframe && button_state_changed(Button::BLUE)) {
            if(button_is(Button::BLUE, HIGH)) {
                skip_day = skip_day == day ? -1 : day;
            }
            debounce = true;
        }

        if (button_is(Button::RED, HIGH) && button_state_changed(Button::RED)) {
            turn_lights_on = !turn_lights_on;
            debounce = true;
        }

        for (int i = 0; i < BTN_COUNT; i++) {
            state.prev_button_state[i] = state.curr_button_state[i];    
        }

        if (debounce) {
            state.last_debounce_start = ms;
        }
    }

    if (turn_lights_on) {
        digitalWrite(RELAY, LOW);
        digitalWrite(RELAY2, LOW);

        analogWrite(LOW1,255);
        analogWrite(HIGH1, 255);
        analogWrite(LOW2, 255);

        return;
    }

    const unsigned int current_step = minute + (hour - HOUR_START) * 60;

    if (is_in_timeframe && skip_day != day) {
        digitalWrite(RELAY, LOW);
        digitalWrite(RELAY2, LOW);

        if (HOUR_FULL <= hour) {
            analogWrite(LOW1,255);
            analogWrite(HIGH1, 255);
            analogWrite(LOW2, 255);

            return;
        }

        const int step_interval = steps_between / 2;

        if (current_step < step_interval) {
            analogWrite(LOW1, (float)current_step / step_interval * 255);
            return;
        }

        if (current_step >= step_interval) {
            analogWrite(LOW1, 255);
            analogWrite(LOW2, (float)(current_step - step_interval) / step_interval * 255);
            return;
        }       
    } else {
        digitalWrite(RELAY, HIGH);
        digitalWrite(RELAY2, HIGH);

        analogWrite(HIGH1, 0);
        analogWrite(LOW2, 0);
        analogWrite(LOW1, 0);
    }    
}

void turn_on_lcd()
{
    digitalWrite(7, HIGH);
    lcd.display();
}

void turn_off_lcd()
{
    digitalWrite(7, LOW);
    lcd.noDisplay();
}

bool button_state_changed(int btn) {
    return state.prev_button_state[btn] != state.curr_button_state[btn];
}

bool button_is(int btn, int val) {
    return state.curr_button_state[btn] == val;
}
