
// Set this flag to 1 to enable periodic logging.  The number that you set will
// be the number of seconds that we sleep before printing the alloc/free stats.
extern unsigned int memory_leak_tool_periodic_logging_interval;

// Set this flag to 1 if you want to reset the alloc/free stats after logging
// them.  This is useful if you want to do something like display a new set of
// stats every 60 seconds.
extern int memory_leak_tool_reset_after_logging;

extern int memory_leak_tool_start(void);
extern void memory_leak_tool_stop(void);

extern void memory_leak_tool_reset_counters(void);

extern int memory_leak_tool_log_data(void);

