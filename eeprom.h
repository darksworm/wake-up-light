#include <EEPROM.h>
#include "definitions.h"

EEPROMState eeprom_state;

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


void read_state_from_eeprom(State& state) 
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

void write_state_to_eeprom(State state)
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
