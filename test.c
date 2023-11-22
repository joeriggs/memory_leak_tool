#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "memory_leak_tool.h"

int main(int argc, char **argv)
{
	int retcode = 0;

	printf("Starting test.\n\n");

	if(memory_leak_tool_init()) {
		fprintf(stderr, "Unable to initialize memory_leak_tool.\n");
		retcode = 1;
	}

	memory_leak_tool_start();

	void *p1 = NULL, *p2 = NULL, *p3 = NULL, *p4 = NULL;
	void *p_array[3];

	printf("Do some allocations =====================================\n");
	p1 = malloc(1);
	p2 = calloc(10, 1);
	int rc = posix_memalign(&p3, 64, 1024);
	p4 = realloc(p1, 1024);
	int i;
	for(i = 0; i < 3; i++) {
		p_array[i] = malloc(123);
	}
	memory_leak_tool_log_data();

	printf("Take a look at the log.  I'll wait for a few seconds.\n");
	sleep(10);

	printf("Do some frees ===========================================\n");
	free(p2);
	free(p3);
	free(p4);
	for(i = 0; i < 3; i++) {
		free(p_array[i]);
	}
	memory_leak_tool_log_data();

	printf("Take a look at the log.  I'll wait for a few seconds.\n");
	sleep(10);

	printf("Stop tracking ===========================================\n");
	memory_leak_tool_stop();
	memory_leak_tool_log_data();

	printf("Take a look at the log.  I'll wait for a few seconds.\n");
	sleep(10);

	printf("Reset tracking ==========================================\n");
	memory_leak_tool_reset();
	memory_leak_tool_log_data();

	printf("Take a look at the log.  I'll wait for a few seconds.\n");
	sleep(10);

	return retcode;
}

