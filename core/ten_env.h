// The environment component of Ten implements the VM's global
// environment, as well as the global stack which is used for
// allocating variables when no fiber is running.
#ifndef ten_env_h
#define ten_env_h

struct EnvState {
    int TODO;
};

// Initialize the component.
void
envInit( State* state );


// Push and pop values to/from the stack.
ten_Tup
envPush( State* state, uint n );

ten_Tup
envTop( State* state, uint n );

void
envPop( State* state );


// Define a new global, initializing it to the given value.
// If the given name/index is already defined then overwrites its
// value and closes its upvalue if one exists.
void
envDefByName( SymT name, TVal val );

void
envDefByIndex( uint index, TVal val );

// Set the value of an existing global.  If none exists with
// the given name/index then throws an error.
void
envSetByName( SymT name, TVal val );

void
envSetByIndex( uint index, TVal val );

// Returns the value of a global.  If none exists with the
// given name/index then returns `udf`.
TVal
envGetByName( SymT name );

TVal
envGetByIndex( uint index );

#endif
