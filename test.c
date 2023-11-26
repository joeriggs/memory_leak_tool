#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "memory_leak_tool.h"

int main(int argc, char **argv)
{
	int retcode = 0;
	int i;

	printf("Starting test.\n\n");

	if(memory_leak_tool_init()) {
		fprintf(stderr, "Unable to initialize memory_leak_tool.\n");
		retcode = 1;
	}

	memory_leak_tool_start();

	void *p1 = NULL, *p2 = NULL, *p3 = NULL, *p4 = NULL;
	void *p_array[3];

	printf("Do some allocations =====================================\n");
	p1 = malloc(1); // 1 byte
	p2 = calloc(10, 1); // 10 bytes
	int rc = posix_memalign(&p3, 64, 1024); // 64 bytes
	p4 = realloc(p1, 1024); // Replace 1 byte with 1024 bytes.
	for(i = 0; i < 3; i++) {
		p_array[i] = malloc(123); // 123 bytes.
	}
	memory_leak_tool_log_data();

	printf("Take a look at the log.  I'll wait for a few seconds.\n");
	sleep(5);

	printf("Do some frees ===========================================\n");
	free(p2);
	free(p3);
	free(p4);
	for(i = 0; i < 1; i++) {
		free(p_array[i]);
	}
	memory_leak_tool_log_data();

	printf("Take a look at the log.  I'll wait for a few seconds.\n");
	sleep(5);

	printf("Stop tracking ===========================================\n");
	memory_leak_tool_stop();
	memory_leak_tool_log_data();

	printf("Take a look at the log.  I'll wait for a few seconds.\n");
	sleep(5);

	printf("Reset tracking ==========================================\n");
	memory_leak_tool_reset();
	memory_leak_tool_log_data();

	printf("Take a look at the log.  I'll wait for a few seconds.\n");
	sleep(60);

	return retcode;
}

