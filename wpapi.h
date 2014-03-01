#include <papi.h>

/*! make sure to update the "events" variable in wpapi.c */
enum  {papi_instructions = 0,
       papi_L3_cache_misses, 
       papi_TLB_data_misses, 
       papi_stall_cycles, 
       papi_stores, 
       papi_loads,
       num_counters};

// High-level interface.  This does auto-initialize.
void start_papi(void);
void stop_papi(long_long *results);
