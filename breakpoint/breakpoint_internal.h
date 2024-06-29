
#ifndef __BREAKPOINT_INTERNAL_H__
#define __BREAKPOINT_INTERNAL_H__

// We store the context for several breakpoints.  The context is passed to
// our breakpoint_handler_set() function.
typedef struct breakpoint_context {
	int used;

	void *bp_entrypoint;
	void *bp_retaddr;

	pre_processor_callback pre_cb;
	post_processor_callback post_cb;
} breakpoint_context;

#endif

