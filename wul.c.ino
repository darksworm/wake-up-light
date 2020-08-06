#include <Wire.h>
#include "RTClib.h"

const int relay2 = 9;
const int relay = 10;
const int timeframe_toggle_button = 11;
const int override_button = 12;
const int high1 = 4;
const int low2 = 6;
const int low1 = 5;
const int led = 13;

const int hour_start = 7;
const int hour_full = 8;
const int hour_end = 9;

const int steps_between = hour_full - hour_start + 1 * 60;

RTC_DS3231 rtc;

int prev_timeframe_toggle_button_state = LOW;
int prev_override_button_state = LOW;
int skipDay = -1;
bool turn_lights_on = false;

void setup () 
{
  delay(700);
  Serial.begin(9600);
  delay(700);
  
  pinMode(led, OUTPUT);
  pinMode(high1, OUTPUT);
  pinMode(low2, OUTPUT);
  pinMode(low1, OUTPUT);
  pinMode(relay, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(timeframe_toggle_button, INPUT);

  analogWrite(led, 0);
  analogWrite(high1, 0);
  analogWrite(low2, 0);
  analogWrite(low1, 0);
  
  analogWrite(relay, 0);
  analogWrite(relay2, 0);

  if (!rtc.begin()) {
    while(1) {
      analogWrite(led, 255);
      delay(750);
      analogWrite(led, 128);
      delay(750);
      analogWrite(led, 0);
      delay(750);
    }
  }

  if (rtc.lostPower()) {
     while(1) {
      analogWrite(led, 0);
      delay(750);
      analogWrite(led, 64);
      delay(750);
      analogWrite(led, 128);
      delay(750);
    }
  }

  // Following line sets the RTC to the date & time this sketch was compiled
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  Serial.println("end of setup");
}

void loop () 
{  
    DateTime now = rtc.now();

    const unsigned int day = now.day();
    const unsigned int hour = now.hour();
    const unsigned int minute = now.minute();

    int timeframe_toggle_button_state = digitalRead(timeframe_toggle_button);
    int override_button_state = digitalRead(override_button);

    bool isInTimeframe = hour >= hour_start && hour < hour_end;

    if (isInTimeframe && prev_timeframe_toggle_button_state != timeframe_toggle_button_state) {
        if(timeframe_toggle_button_state == HIGH) {
           skipDay = skipDay == day ? -1 : day;
        }
        
        prev_timeframe_toggle_button_state = timeframe_toggle_button_state;
        delay(50); //debounce
    }
    
    if (override_button_state == HIGH && prev_override_button_state != HIGH) {
        turn_lights_on = !turn_lights_on;
        delay(50); //debounce
    }
    
    prev_override_button_state = override_button_state;

    if (turn_lights_on) {
        digitalWrite(relay, LOW);
        digitalWrite(relay2, LOW);
        
        analogWrite(low1,255);
        analogWrite(high1, 255);
        analogWrite(low2, 255);
          
        return;
    }

    const unsigned int current_step = minute + (hour - hour_start) * 60;
      
    if (isInTimeframe && skipDay != day) {
       digitalWrite(relay, LOW);
       digitalWrite(relay2, LOW);

       if (hour_full <= hour) {
          analogWrite(low1,255);
          analogWrite(high1, 255);
          analogWrite(low2, 255);
          
          return;
       }

       const int step_interval = steps_between / 2;

       if (current_step < step_interval) {
          analogWrite(low1, (float)current_step / step_interval * 255);
          return;
       }

       if (current_step >= step_interval) {
          analogWrite(low1, 255);
          analogWrite(low2, (float)(current_step - step_interval) / step_interval * 255);
          return;
       }       
    } else {
       digitalWrite(relay, HIGH);
       digitalWrite(relay2, HIGH);
       
       analogWrite(high1, 0);
       analogWrite(low2, 0);
       analogWrite(low1, 0);
    }    
}
