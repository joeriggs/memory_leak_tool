
/* Initialize the memory_leak_tool.  You need to call this function before you
 * try to do anything else with the memory_leak_tool.
 *
 * Note that this doesn't start any tracking activities.  You need to call
 * memory_leak_tool_start() in order to start tracking malloc/free activity.
 *
 * Returns:
 *   0 = success.
 *   1 = failure.
 */
extern int memory_leak_tool_init(void);

extern int memory_leak_tool_start(void);
extern int memory_leak_tool_stop(void);

extern int memory_leak_tool_reset(void);

extern int memory_leak_tool_log_data(void);

