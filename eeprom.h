#include <EEPROM.h>
#include "definitions.h"

EEPROMState eeprom_state;

#define COMMA ,
#define DECL_STATEVARS(var_name, state_var_name)  \
    int* var_name[] = { \
        STORABLE_STATE_VARIABLES(&state_var_name.,COMMA)\
    };

#define DECL_VARS \
    DECL_STATEVARS(state_variables, state); \
    DECL_STATEVARS(eeprom_state_variables, eeprom_state); \
    static_assert(sizeof(state_variables) == sizeof(eeprom_state_variables), \
            "State variable count doesnt match eeprom state variable count"); \
    static_assert(sizeof(eeprom_default_state_vals) / sizeof(int) == \
            sizeof(eeprom_state_variables) / sizeof(int*), \
            "eeporom state var count doesn't match default value count")

int eeprom_default_state_vals[] = { 
    1,      // clock enabled
    7 * 60, // start at 7AM (minutes/10)
    60,     // ramp for 1hr (minutes/10)
    9 * 60, // end at 9AM (minutes/10)
    0,      // no auto adjustment
    7 * 60, // target start time same as start time
    8       // adjusted on non-existant day
};

// TODO: clean these up
void read_state_from_eeprom(State& state) 
{
    EEPROM.get(STATE_ADDR, eeprom_state);
    DECL_VARS;

    for (int i = 0; i < sizeof(state_variables) / sizeof(int*); i++) {
        *state_variables[i] = *eeprom_state_variables[i];
    }
}

void write_state_to_eeprom(State state)
{
    DECL_VARS;
    
    for (int i = 0; i < sizeof(state_variables) / sizeof(int*); i++) {
        *eeprom_state_variables[i] = *state_variables[i];
    }

    EEPROM.put(STATE_ADDR, eeprom_state);
}

void initialize_default_state_to_eeprom()
{
    DECL_STATEVARS(eeprom_state_variables, eeprom_state);

    for (int i = 0; i < sizeof(eeprom_state_variables) / sizeof(int); i++) {
        *eeprom_state_variables[i] = eeprom_default_state_vals[i];
    }

    EEPROM.put(STATE_ADDR, eeprom_state);
}

bool eeprom_is_initialized()
{
    return EEPROM.read(MAGIC_NUMBER_ADDR) == MAGIC_NUMBER;
}
