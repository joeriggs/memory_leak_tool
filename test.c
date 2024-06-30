#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>

#include "memory_leak_tool.h"

static int mmap_test_fd = -1;
static unsigned char *mmap_test_ptr = NULL;

static int mmap_test_init(void)
{
	int retcode = 1;

	do {
		// Create a shared memory space.
		mmap_test_fd = shm_open("my_shm", O_RDWR | O_CREAT, 0666);
		if(mmap_test_fd < 0) {
			printf("%s(): shm_open() failed (%m).\n", __FUNCTION__);
			break;
		}

		// Success.
		retcode = 0;
	} while(0);

	return retcode;
}

static int mmap_test_alloc(void)
{
	int retcode = 1;

	do {
		if (mmap_test_fd < 0) {
			printf("%s(): mmap_test_fd < 0.\n", __FUNCTION__);
			break;
		}

		// Call mmap() to map the memory.
		//
		// This is the point where our breakpoint code should inject itself
		// into the flow.
#define __USE_GNU
#include <ucontext.h>
		ucontext_t ucp;
		getcontext(&ucp);
		mmap_test_ptr = mmap(0, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_test_fd, 0);
		if (!mmap_test_ptr) {
			printf("%s(): mmap() failed (%m).\n", __FUNCTION__);
			break;
		}
		printf("mmap() returned %p.\n", mmap_test_ptr);

		retcode = 0;
	} while(0);

	return retcode;
}

static int mmap_test_free(void)
{
	int retcode = 1;

	do {
		if (mmap_test_fd < 0) {
			printf("%s(): mmap_test_fd < 0.\n", __FUNCTION__);
			break;
		}

		// Unmap the memory.  Once again we should inject ourselves.
		int rc = munmap(mmap_test_ptr, 65536);
		if (rc == -1) {
			printf("%s(): munmap() failed (%m).\n", __FUNCTION__);
			break;
		}

		// Success.
		return 0;
	} while(0);

	return retcode;
}

static int mmap_test_cleanup(void)
{
	int retcode = 0;

	close(mmap_test_fd);

	return retcode;
}

int main(int argc, char **argv)
{
	int retcode = 0;
	int rc;
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
	rc = posix_memalign(&p3, 64, 1024); // 64 bytes
	p4 = realloc(p1, 1024); // Replace 1 byte with 1024 bytes.
	for(i = 0; i < 3; i++) {
		p_array[i] = malloc(123); // 123 bytes.
	}

	mmap_test_init();
	mmap_test_alloc();

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
	mmap_test_free();
	mmap_test_cleanup();
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
	sleep(5);

	return retcode;
}

