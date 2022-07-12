#include <malloc.h>
#include <stdio.h>

#include "memory_leak_tool.h"

int main(int argc, char **argv)
{
	int retcode = 0;

	if(memory_leak_tool_start()) {
		fprintf(stderr, "Unable to initialize memory_leak_tool.\n");
		retcode = 1;
	}

	void *ptr1 = malloc(1024);
	void *ptr2 = malloc(1024);

	free(ptr1);
	free(ptr2);

	memory_leak_tool_log_data();

	return retcode;
}

