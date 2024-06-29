
#ifndef __BREAKPOINT_H__
#define __BREAKPOINT_H__

#include <stdint.h>

typedef int (*pre_processor_callback)
	(uint64_t arg_01, uint64_t arg_02, uint64_t arg_03, uint64_t arg_04, uint64_t arg_05,
	 uint64_t arg_06, uint64_t arg_07, uint64_t arg_08, uint64_t arg_09, uint64_t arg_10);

typedef int (*post_processor_callback) (uint64_t retcode);

int breakpoint_handler_init(void);

int breakpoint_handler_set(void *addr,
                           pre_processor_callback pre_cb,
                           post_processor_callback post_cb);

#endif

