#include <EEPROM.h>
#include "definitions.h"

#define COMMA ,
#define DECL_STATEVARS(var_name, state_var_name)  \
    int* var_name[] = { \
        STORABLE_STATE_VARIABLES(&state_var_name.,COMMA)\
    };

#define DECL_VARS \
    DECL_STATEVARS(state_variables, state); \
    static_assert(sizeof(state_variables) / sizeof(int*) \
            == sizeof(eeprom_default_state_vals) / sizeof(int), \
            "State variable count doesnt match eeprom state variable count");

#define STATE_SIZE sizeof(eeprom_default_state_vals) / sizeof(int)

int eeprom_default_state_vals[] = { 
    1,      // clock enabled
    7 * 60, // start at 7AM (minutes/10)
    60,     // ramp for 1hr (minutes/10)
    9 * 60, // end at 9AM (minutes/10)
    0,      // no auto adjustment
    7 * 60, // target start time same as start time
    8       // adjusted on non-existant day
};

void read_state_from_eeprom(State& state) 
{
    DECL_VARS;
    int read_state[STATE_SIZE];

    EEPROM.get(STATE_ADDR, read_state);

    for (int i = 0; i < STATE_SIZE; i++) {
        *state_variables[i] = read_state[i];
    }
}

void write_state_to_eeprom(State state)
{
    DECL_VARS;
    int writable_state[STATE_SIZE];
    
    for (int i = 0; i < STATE_SIZE; i++) {
        writable_state[i] = *state_variables[i];
    }

    EEPROM.put(STATE_ADDR, writable_state);
}

void initialize_default_state_to_eeprom()
{
    EEPROM.put(STATE_ADDR, eeprom_default_state_vals);
}

bool eeprom_is_initialized()
{
    return EEPROM.read(MAGIC_NUMBER_ADDR) == MAGIC_NUMBER;
}
