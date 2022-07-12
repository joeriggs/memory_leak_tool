#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "memory_leak_tool.h"

// These are the functions that we're overriding.
extern void * __libc_malloc(size_t size);
extern void * __libc_calloc(size_t nmemb, size_t size);
extern void * __libc_realloc(void *ptr, size_t size);
extern void * __libc_memalign(size_t alignment, size_t size);
extern void   __libc_free(void *ptr);

// Set this flag to 1 to enable the alloc/free counters.
static int tool_enabled = 0;

// Set this flag to 1 to enable periodic logging.  The number that you set will
// be the number of seconds that we sleep before printing the alloc/free stats.
static unsigned int periodic_logging_interval = 0;

// Set this flag to 1 if you want to reset the alloc/free stats after logging
// them.  This is useful if you want to do something like display a new set of
// stats every 60 seconds.
static int reset_after_logging = 0;

// We create a thread that can be used to periodically log alloc/free data.
// This is the PID of that thread.  It's not useful, but we keep it anyway.
static pthread_t thread_id;

// This mutex is used to protect the counters when multiple threads are running.
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

// The total number of times that all of the various alloc functions were
// called, and the total number of bytes requested by the calls.
static int      allocate_counter = 0;
static uint64_t allocate_bytes = 0;

// The total number of times that free() was called.
static int      free_counter = 0;

// The data for each caller is kept in a series of arrays.  This macro specifies
// how many alloc and free callers we maintain.  If more than that shows up, then
// we don't maintains counters for them.
//
// This is an artitrary number.  Feel free to increase or decrease it.
#define MAX_CALLERS 100

// The *_ptrs array contains the PC address of a function that called an
// alloc function.
// The corresponding *_counts array contains the number of times that this
// particular PC address called an alloc function.
static void    *alloc_callers_ptrs[MAX_CALLERS];
static uint64_t alloc_callers_counts[MAX_CALLERS];

// The *_ptrs array contains the PC address of a function that called free().
// The corresponding *_counts array contains the number of times that this
// particular PC address called free().
static void    *free_callers_ptrs[MAX_CALLERS];
static uint64_t free_callers_counts[MAX_CALLERS];

// You can ask for additional details.
#define MAX_NUM_DETAIL_PTRS 1024
static int     details_index = 0;
static void   *details_ptrs[MAX_NUM_DETAIL_PTRS];
static void   *details_pcs[MAX_NUM_DETAIL_PTRS];
static size_t  details_sizes[MAX_NUM_DETAIL_PTRS];
static char    details_events[MAX_NUM_DETAIL_PTRS];

/*******************************************************************************
 * This function is called each time one of the alloc functions is called.
 ******************************************************************************/
static void record_alloc_event(void *caller, size_t size, void *ptr)
{
	int i;

	if (tool_enabled) {
		pthread_mutex_lock(&data_mutex);

		allocate_counter++;
		allocate_bytes += size;

		for(i = 0; i < MAX_CALLERS; i++) {
			if(alloc_callers_ptrs[i] == caller) {
				alloc_callers_counts[i]++;
				break;
			}
			if(alloc_callers_ptrs[i] == NULL) {
				alloc_callers_ptrs[i] = caller;
				alloc_callers_counts[i]++;
				break;
			}
		}

		pthread_mutex_unlock(&data_mutex);
	}
}

/*******************************************************************************
 * This function is called each time free() is called.
 ******************************************************************************/
static void record_free_event(void *caller, void *ptr)
{
	if(ptr == NULL)
		return;

	if (tool_enabled) {
		pthread_mutex_lock(&data_mutex);

		free_counter++;

		int i;
		for(i = 0; i < MAX_CALLERS; i++) {
			if(free_callers_ptrs[i] == caller) {
				free_callers_counts[i]++;
				break;
			}
			if(free_callers_ptrs[i] == NULL) {
				free_callers_ptrs[i] = caller;
				free_callers_counts[i]++;
				break;
			}
		}

		pthread_mutex_unlock(&data_mutex);
	}
}

/*******************************************************************************
 * These are the libc alloc/free functions that we override.
 ******************************************************************************/

void *malloc(size_t size)
{
	void *retAddr = __builtin_return_address(0);
	void *ptr = __libc_malloc(size);
	record_alloc_event(retAddr, size, ptr);
	return ptr;
}

void *calloc(size_t nmemb, size_t size)
{
	void *retAddr = __builtin_return_address(0);
	void *ptr = __libc_calloc(nmemb, size);
	record_alloc_event(retAddr, size, ptr);
	return ptr;
}

void *realloc(void *ptr, size_t size)
{
	void *retAddr = __builtin_return_address(0);
	void *new_ptr = __libc_realloc(ptr, size);
	record_alloc_event(retAddr, size, new_ptr);
	return new_ptr;
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	void *retAddr = __builtin_return_address(0);
	*memptr = __libc_memalign(alignment, size);
	record_alloc_event(retAddr, size, memptr);
	if(*memptr != NULL) {
		return 0;
	}
	else {
		return ENOMEM;
	}
}

void *aligned_alloc(size_t alignment, size_t size)
{
	void *retAddr = __builtin_return_address(0);
	void *ptr = __libc_memalign(alignment, size);
	record_alloc_event(retAddr, size, ptr);
	return ptr;
}

void free(void *ptr)
{
	void *retAddr = __builtin_return_address(0);
	record_free_event(retAddr, ptr);
	return __libc_free(ptr);
}

/*******************************************************************************
 * Reset all of the alloc/free counters.  This is useful if you want to run a
 * single test and evaluate the counters for that test.
 ******************************************************************************/
static void reset_counters(int alreadyLocked)
{
	int i;

	if(!alreadyLocked)
		pthread_mutex_lock(&data_mutex);

	allocate_counter = 0;
	allocate_bytes = 0;
	free_counter = 0;

	for(i = 0; i < MAX_CALLERS; i++) {
		alloc_callers_ptrs[i]     = 0;
		alloc_callers_counts[i]   = 0;
		free_callers_ptrs[i]      = 0;
		free_callers_counts[i]    = 0;
	}

	details_index = 0;
	for(i = 0; i < MAX_NUM_DETAIL_PTRS; i++) {
		details_ptrs[i]   = NULL;
		details_pcs[i]    = NULL;
		details_sizes[i]  = 0;
		details_events[i] = ' ';
	}

	if(!alreadyLocked)
		pthread_mutex_unlock(&data_mutex);
}

/*******************************************************************************
 * This is a thread that can be used to periodically log memory_leak_tool data.
 ******************************************************************************/
static int thread_func(void *arg)
{
	(void) arg;
	unsigned int sleepInterval = 0;

	while(tool_enabled) {
		sleep(1);

		if((periodic_logging_interval > 0) && (++sleepInterval >= periodic_logging_interval)) {
			memory_leak_tool_log_data();
			sleepInterval = 0;
		}
	}

	return 0;
}

/*******************************************************************************
 *******************************************************************************
 ******************************* Public functions ******************************
 *******************************************************************************
 ******************************************************************************/


/*******************************************************************************
 * Call this function to start the memory_leak_tool.
 *
 * Output:
 *   0 = success.
 *   1 = failure.
 ******************************************************************************/
int memory_leak_tool_start(void)
{
	reset_counters(0);

	tool_enabled = 1;

	pthread_attr_t tattr;
	if(pthread_attr_init(&tattr)) {
		fprintf(stderr, "ERROR: thread attr init fail (%m).");
		return 1;
	}
	if(pthread_create(&thread_id, &tattr, (void *) thread_func, NULL)) {
		fprintf(stderr, "ERROR: thread start fail (%m).");
		return 1;
	}
	if(pthread_attr_destroy(&tattr)) {
		fprintf(stderr, "ERROR: thread attr destroy fail (%m).");
		return 1;
	}

	return 0;
}

/*******************************************************************************
 * Call this function to stop the memory_leak_tool.
 *
 * Output:
 *   0 = success.
 *   1 = failure.
 ******************************************************************************/
void memory_leak_tool_stop(void)
{
	tool_enabled = 0;
}

/*******************************************************************************
 * Reset the internal counters. 
 ******************************************************************************/
void memory_leak_tool_reset_counters(void)
{
	reset_counters(0);
}

/*******************************************************************************
 * Log the current set of alloc/free data.
 ******************************************************************************/
int memory_leak_tool_log_data(void)
{
	void    *tmp_alloc_callers_ptrs[MAX_CALLERS];
	uint64_t tmp_alloc_callers_counts[MAX_CALLERS];

	void    *tmp_free_callers_ptrs[MAX_CALLERS];
	uint64_t tmp_free_callers_counts[MAX_CALLERS];

	int      tmp_details_Index;
	void    *tmp_details_ptrs[MAX_NUM_DETAIL_PTRS];
	void    *tmp_details_pcs[MAX_NUM_DETAIL_PTRS];
	size_t   tmp_details_sizes[MAX_NUM_DETAIL_PTRS];
	char     tmp_details_events[MAX_NUM_DETAIL_PTRS];

	int a_counter, f_counter;
	uint64_t a_bytes;
	int allocIndex, freeIndex;

	memset(tmp_alloc_callers_ptrs,       0, sizeof(tmp_alloc_callers_ptrs));
	memset(tmp_alloc_callers_counts,     0, sizeof(tmp_alloc_callers_counts));
	memset(tmp_free_callers_ptrs,        0, sizeof(tmp_free_callers_ptrs));
	memset(tmp_free_callers_counts,      0, sizeof(tmp_free_callers_counts));
	memset(tmp_details_ptrs,             0, sizeof(tmp_details_ptrs));
	memset(tmp_details_pcs,              0, sizeof(tmp_details_pcs));
	memset(tmp_details_sizes,            0, sizeof(tmp_details_sizes));
	memset(tmp_details_events,         ' ', sizeof(tmp_details_events));

	pthread_mutex_lock(&data_mutex);

	a_counter = allocate_counter;
	a_bytes = allocate_bytes;
	f_counter = free_counter;

	for(allocIndex = 0; (allocIndex < MAX_CALLERS) && (alloc_callers_ptrs[allocIndex] != NULL); allocIndex++) {
		tmp_alloc_callers_ptrs[allocIndex] = alloc_callers_ptrs[allocIndex];
		tmp_alloc_callers_counts[allocIndex] = alloc_callers_counts[allocIndex];
	}

	for(freeIndex = 0; (freeIndex < MAX_CALLERS) && (free_callers_ptrs[freeIndex] != NULL); freeIndex++) {
		tmp_free_callers_ptrs[freeIndex] = free_callers_ptrs[freeIndex];
		tmp_free_callers_counts[freeIndex] = free_callers_counts[freeIndex];
	}

	tmp_details_Index = details_index;

	int i;
	for(i = 0; i < tmp_details_Index; i++) {
		tmp_details_ptrs[i]   = details_ptrs[i];
		tmp_details_pcs[i]    = details_pcs[i];
		tmp_details_sizes[i]  = details_sizes[i];
		tmp_details_events[i] = details_events[i];
	}

	// Optionally reset the counters.
	if(reset_after_logging) {
		reset_counters(1);
	}

	pthread_mutex_unlock(&data_mutex);

	int fd = open("/tmp/memory_leak_tool.log", O_RDWR | O_APPEND | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);

	char buf[1024];

	snprintf(buf, sizeof(buf), "alloc callers [%d] : free callers [%d] : allocs [%d] : bytes [%lu] : frees [%d].\n",
	         allocIndex, freeIndex, a_counter, a_bytes, f_counter);
	write(fd, buf, strlen(buf));

	snprintf(buf, sizeof(buf), "alloc counters ---------------------------------------------------------\n");
	write(fd, buf, strlen(buf));

	for(i = 0; i < allocIndex; i++) {
		snprintf(buf, sizeof(buf), "[%d] : Caller [%p] : Count [%lu].\n", i,
		         tmp_alloc_callers_ptrs[i], tmp_alloc_callers_counts[i]);
		write(fd, buf, strlen(buf));
	}

	snprintf(buf, sizeof(buf), "free counters ---------------------------------------------------------\n");
	write(fd, buf, strlen(buf));

	for(i = 0; i < freeIndex; i++) {
		snprintf(buf, sizeof(buf), "[%d] : Caller [%p] : Count [%lu].\n", i,
		         tmp_free_callers_ptrs[i], tmp_free_callers_counts[i]);
		write(fd, buf, strlen(buf));
	}

	snprintf(buf, sizeof(buf), "Details ----------------------------------------------------------------\n");
	write(fd, buf, strlen(buf));

	for(i = 0; i < tmp_details_Index; i++) {
		snprintf(buf, sizeof(buf), "[%c] [%p] [%p] [%lu]\n", tmp_details_events[i], tmp_details_pcs[i],
		         tmp_details_ptrs[i], tmp_details_sizes[i]);
		write(fd, buf, strlen(buf));
	}

	snprintf(buf, sizeof(buf), "mallinfo() -------------------------------------------------------------\n");
	write(fd, buf, strlen(buf));

	struct mallinfo mInfo = mallinfo();
	snprintf(buf, sizeof(buf),
	       "arena = %u, ordblks = %u, smblks = %u, hblks = %u, hblkhd = %u, usmblks = %u, fsmblks = %u, uordblks = %u, fordblks = %u, keepcost = %u\n",
	        mInfo.arena, mInfo.ordblks, mInfo.smblks, mInfo.hblks, mInfo.hblkhd,
	        mInfo.usmblks, mInfo.fsmblks, mInfo.uordblks, mInfo.fordblks, mInfo.keepcost);
	write(fd, buf, strlen(buf));

	snprintf(buf, sizeof(buf), "========================================================================\n");
	write(fd, buf, strlen(buf));

	int closeResult = close(fd);

	return 0;
}

