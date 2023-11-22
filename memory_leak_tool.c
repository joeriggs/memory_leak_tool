
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include <sys/types.h>

#include "memory_leak_tool.h"

#ifndef RTLD_NEXT
#define RTLD_NEXT  ((void *) -1l)
#endif

#define MEM_HOOK_LOGGER printf
#define LOG_FILE "/tmp/malloc.log"
static const char *separator = "=========================================================";

// This is set to 1 when the memory_leak_tool has been initialized.
static int moduleInitialized = 0;

// This is set to 1 when the user wants us to start tracking malloc/free events.
static int memoryHooksEnabled = 0;

// We set this to 1 while we're adding an entry to our internal database,
// removing an entry from the internal database, or dumping the internal
// database to the log file.  It prevents us from being re-entered, which could
// result in an infinite loop.
static __thread int processingAnOperation = 0;

// These are pointers to the functions that we've overloaded.
static void *(*__calloc)(size_t number, size_t size) = NULL;
static void *(*__malloc)(size_t size) = NULL;
static int   (*__posix_memalign)(void **ptr, size_t alignment, size_t size) = NULL;
static void *(*__realloc)(void *ptr, size_t size) = NULL;
static void  (*__free)(const void *addr) = NULL;

// We use one alloc_event object for each malloc operation that we're tracking.
// This bunch of code defines the object, creates an array of objects that we
// use to store the events that we're tracking, and a few counters.
#define NUM_CALLERS 32
typedef struct alloc_event {
	size_t num_callers;
	void *callers[NUM_CALLERS];
	void *ptr;
	size_t size;
	int reference_count;
	TAILQ_ENTRY(alloc_event) tailq;
	struct alloc_event *caller_event;
} alloc_event;
#define MY_QUEUE_SIZE 1000000
static alloc_event alloc_event_array[MY_QUEUE_SIZE];
static int alloc_event_adds = 0;
static int alloc_event_dels = 0;
static uint64_t total_bytes_allocated = 0;
static uint64_t total_bytes_freed = 0;

// This is the definition of the alloc_event_queue data type.  We create 3
// queues of this type:
// 1. free_event_queue    - A list of unused entries.
// 2. used_event_queue    - A hash table that contains the currently-tracked
//                          malloc events.  The allocated memory address is used
//                          as the key.
// 3. callers_event_queue - A hash table that contains the currently-tracked
//                          malloc events.  The caller's PC is used as the key.
TAILQ_HEAD(alloc_event_queue, alloc_event);

// This is a queue of unused entries.
static struct alloc_event_queue free_event_queue;
static pthread_mutex_t          free_event_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

// This is 2 hash tables that contain the list of current malloc events.
// 1. used_event_queue uses the allocated memory address as the key.
// 2. callers_event_queue uses the PC of the caller as the key.
// address of the allocated memory is used as the key.
#define EVENT_QUEUE_NUM_BUCKETS 256
#define EVENT_QUEUE_BUCKET_MASK (EVENT_QUEUE_NUM_BUCKETS - 1)
static struct alloc_event_queue used_event_queue[EVENT_QUEUE_NUM_BUCKETS];
static struct alloc_event_queue callers_event_queue[EVENT_QUEUE_NUM_BUCKETS];
static pthread_mutex_t          used_event_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

// On Linux, calling dlsym() to get the address of the "calloc" function
// results in calloc() getting called again.  This is an array of buffers that
// can be used to satisfy calloc() calls until dlsym() returns.  Once dlsym()
// returns the address of the real calloc() function, then these buffers are
// no longer used.  I think 1 buffer is sufficient, but we'll hang on to 100,
// just in case.
#define STATIC_CALLOC_BUF_SIZE 1024
static unsigned char callocBuf[100][STATIC_CALLOC_BUF_SIZE];
static void *callocBufBeg = &callocBuf[0][0];
static void *callocBufEnd = &callocBuf[99][STATIC_CALLOC_BUF_SIZE - 1];

/* ************************************************************************** */
/* ************************************************************************** */
/* **************** THESE ARE SOME PRIVATE UTILITY FUNCTIONS **************** */
/* ************************************************************************** */
/* ************************************************************************** */

/* Add a malloc event to the hash table.
 */
static void alloc_event_add(void *ptr, size_t size)
{
	// If we're already processing an operation, just return.  We don't want
	// to allow an infinite loop.
	if (processingAnOperation)
		return;
	processingAnOperation = 1;

	if (memoryHooksEnabled) {
		/* Get an unused entry. */
		pthread_mutex_lock(&free_event_queue_mutex);
		if (TAILQ_EMPTY(&free_event_queue)) {
			MEM_HOOK_LOGGER("Ran out of slots.\n");
		}
		struct alloc_event *new_entry = TAILQ_FIRST(&free_event_queue);
		TAILQ_REMOVE(&free_event_queue, new_entry, tailq);
		pthread_mutex_unlock(&free_event_queue_mutex);

		/* Get the backtrace to this call. */
		void *tracePtrs[100];
		int num_callers = backtrace(tracePtrs, 100);
		if ((num_callers == 0) || (num_callers > 100)) {
			MEM_HOOK_LOGGER("SPOTTED AN ERROR (%d) (%m).", num_callers);
		}
		/* The first 2 callers are our malloc_hook functions.  Skip them. */
		new_entry->num_callers = num_callers - 2;
		int x;
		for (x = 0; (x < NUM_CALLERS) && (x < num_callers); x++) {
			new_entry->callers[x] = tracePtrs[x + 2];
		}
		new_entry->ptr = ptr;
		new_entry->size = size;

		uint64_t used_event_queue_bucket    = ((uint64_t) ptr                   >> 8) % EVENT_QUEUE_BUCKET_MASK;
		uint64_t callers_event_queue_bucket = ((uint64_t) new_entry->callers[0] >> 0) % EVENT_QUEUE_BUCKET_MASK;

		pthread_mutex_lock(&used_event_queue_mutex);

		// Place the entry in the used_event_queue.
		TAILQ_INSERT_TAIL(&used_event_queue[used_event_queue_bucket], new_entry, tailq);
		alloc_event_adds++;
		total_bytes_allocated += size;

		// Look through the callers_event_queue to see if this caller backtrace
		// is already in the list.
		// 1. If it's in the list, then increment the reference_count.
		// 2. If it's not in the list, then add it.
		struct alloc_event *new_caller_entry;
		int found_duplicate_entry = 0;
		TAILQ_FOREACH(new_caller_entry, &callers_event_queue[callers_event_queue_bucket], tailq) {
			if ( (new_caller_entry->num_callers == new_entry->num_callers) &&
			     (new_caller_entry->size        == new_entry->size) ) {

				int i;
				for(i = 0; i < new_caller_entry->num_callers; i++) {
					if (new_caller_entry->callers[i] != new_entry->callers[i]) {
						break;
					}
				}

				if (i == new_caller_entry->num_callers) {
					new_caller_entry->reference_count++;
					new_entry->caller_event = new_caller_entry;
					found_duplicate_entry = 1;
				}
			}
		}

		// If this caller backtrace isn't already in the callers_event_queue,
		// then allocate a new entry and insert it into the callers_event_queue.
		if (!found_duplicate_entry) {
			pthread_mutex_lock(&free_event_queue_mutex);
			if (TAILQ_EMPTY(&free_event_queue)) {
				MEM_HOOK_LOGGER("Ran out of slots.\n");
			}
			new_caller_entry = TAILQ_FIRST(&free_event_queue);
			TAILQ_REMOVE(&free_event_queue, new_caller_entry, tailq);
			pthread_mutex_unlock(&free_event_queue_mutex);

			memcpy(new_caller_entry, new_entry, sizeof(*new_entry));
			new_caller_entry->reference_count = 1;
			new_entry->caller_event = new_caller_entry;
			TAILQ_INSERT_TAIL(&callers_event_queue[callers_event_queue_bucket], new_caller_entry, tailq);
		}

		pthread_mutex_unlock(&used_event_queue_mutex);
	}

	processingAnOperation = 0;
}

/* Remove a malloc event from the hash table.
 */
static void alloc_event_del(void *ptr)
{
	// If we're already processing an operation, just return.  We don't want
	// to allow an infinite loop.
	if (processingAnOperation)
		return;
	processingAnOperation = 1;

	if (memoryHooksEnabled) {
		uint64_t used_event_queue_bucket = ((uint64_t) ptr >> 8) % EVENT_QUEUE_BUCKET_MASK;

		// Locate the used_event_queue entry for "ptr", and remove it
		// from the used_event_queue.
		struct alloc_event *used_event_queue_entry = NULL;
		struct alloc_event *__tmp_entry;
		pthread_mutex_lock(&used_event_queue_mutex);
		TAILQ_FOREACH(__tmp_entry, &used_event_queue[used_event_queue_bucket], tailq) {
			if (__tmp_entry->ptr == ptr) {
				total_bytes_freed += __tmp_entry->size;
				alloc_event_dels++;
				TAILQ_REMOVE(&used_event_queue[used_event_queue_bucket], __tmp_entry, tailq);
				used_event_queue_entry = __tmp_entry;
				break;
			}
		}
		pthread_mutex_unlock(&used_event_queue_mutex);

		if (used_event_queue_entry) {

			uint64_t callers_event_queue_bucket = ((uint64_t) used_event_queue_entry->callers[0] >> 0) % EVENT_QUEUE_BUCKET_MASK;

			// Get the callers_event_queue entry for the backtrace, decrement the
			// reference_count, and remove it if the reference_count is zero.
			struct alloc_event *callers_event_queue_entry = used_event_queue_entry->caller_event;
			if (callers_event_queue_entry) {
				callers_event_queue_entry->reference_count--;

				if (!callers_event_queue_entry->reference_count) {
					pthread_mutex_lock(&used_event_queue_mutex);
					TAILQ_REMOVE(&callers_event_queue[callers_event_queue_bucket], callers_event_queue_entry, tailq);
					pthread_mutex_unlock(&used_event_queue_mutex);

					pthread_mutex_lock(&free_event_queue_mutex);
					TAILQ_INSERT_TAIL(&free_event_queue, callers_event_queue_entry, tailq);
					pthread_mutex_unlock(&free_event_queue_mutex);
				}
			}
		}
	}

	processingAnOperation = 0;
}

/* This is a thread that you can start.  It allows you to externally control the
 * memory_leak_tool by using "touch" to create files in the /tmp directory.
 */
static void *malloc_hooks_thread(void *arg)
{
	char *gp_path = (char *)arg;

	while(1) {
		sleep(1);

		struct stat s;
		if (stat("/tmp/doq", &s) == 0) {
			MEM_HOOK_LOGGER("Start the queue.");
			unlink("/tmp/doq");
			memoryHooksEnabled = 1;
		}
		if (stat("/tmp/noq", &s) == 0) {
			MEM_HOOK_LOGGER("Stop the queue.");
			unlink("/tmp/noq");
			memoryHooksEnabled = 0;
		}
		if (stat("/tmp/print", &s) == 0) {
			MEM_HOOK_LOGGER("Print the queue.");
			unlink("/tmp/print");
			memory_leak_tool_log_data();
		}
		if (stat("/tmp/reset", &s) == 0) {
			MEM_HOOK_LOGGER("Reset the queue.");
			unlink("/tmp/reset");
			memory_leak_tool_reset();
		}
	}

	return NULL;
}

/* ************************************************************************** */
/* ************************************************************************** */
/* ************ THESE ARE THE OVERLOADED ALLOC AND FREE FUNCTIONS *********** */
/* ************************************************************************** */
/* ************************************************************************** */

void *calloc(size_t number, size_t size)
{
	if (__calloc == NULL) {
		static int called_dlsym = 0;

		/* We need to make sure we only call dlsym() once.  dlsym() is
		 * going to eventually call us again, so we need to make sure we
		 * don't get caught in a loop. */
		if (!called_dlsym) {
			called_dlsym = 1;
			__calloc = dlsym(RTLD_NEXT, "calloc");
		}

		else {
			/* While dlsym() is fetching the address of the "calloc"
			 * function for us, we will handle any interim calls to
			 * calloc() by returning the addresses of some static
			 * arrays. */
			if (size > STATIC_CALLOC_BUF_SIZE) {
				static char *crashPtr = NULL;
				*crashPtr = 0;
			}

			static int index = 0;
			unsigned char *b = &callocBuf[index++][0];

			memset(b, 0, STATIC_CALLOC_BUF_SIZE);
			return b;
		}
	}

	void *ptr = __calloc(number, size);

	alloc_event_add(ptr, size);

	return ptr;
}

void *malloc(size_t size)
{
	if (__malloc == NULL)
		__malloc = dlsym(RTLD_NEXT, "malloc");

	void *ptr = __malloc(size);

	alloc_event_add(ptr, size);

	return ptr;
}

int posix_memalign(void **ptr, size_t alignment, size_t size)
{
	if (__posix_memalign == NULL)
		__posix_memalign = dlsym(RTLD_NEXT, "posix_memalign");

	int rc = __posix_memalign(ptr, alignment, size);

	alloc_event_add(*ptr, size);

	return rc;
}

void *realloc(void *ptr, size_t size)
{
	if (__realloc == NULL)
		__realloc = dlsym(RTLD_NEXT, "realloc");

	void *new_ptr = __realloc(ptr, size);

	alloc_event_add(new_ptr, size);

	return new_ptr;
}

void free(void *ptr)
{
	/* If the caller is freeing one of our static buffers, then return. */
	if ((ptr >= callocBufBeg) && (ptr <= callocBufEnd))
		return;

	if (__free == NULL)
		__free = dlsym(RTLD_NEXT, "free");

	__free(ptr);

	alloc_event_del(ptr);
}

/* ************************************************************************** */
/* ************************************************************************** */
/* **************************** PUBLIC FUNCTIONS **************************** */
/* ************************************************************************** */
/* ************************************************************************** */

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
int memory_leak_tool_init(void)
{
	int retcode = 0;

	int i;
	for (i = 0; i < EVENT_QUEUE_NUM_BUCKETS; i++) {
		TAILQ_INIT(&used_event_queue[i]);
		TAILQ_INIT(&callers_event_queue[i]);
	}

	pthread_mutex_lock(&free_event_queue_mutex);
	TAILQ_INIT(&free_event_queue);
	for (i = 0; i < MY_QUEUE_SIZE; i++) {
		TAILQ_INSERT_TAIL(&free_event_queue, &alloc_event_array[i], tailq);
	}
	pthread_mutex_unlock(&free_event_queue_mutex);

	pthread_t tid;
	retcode = pthread_create(&tid, NULL, malloc_hooks_thread, NULL);
	if (retcode) {
		MEM_HOOK_LOGGER("pthread_create() failed (%m).");
	}

	moduleInitialized = 1;

	return retcode;
}

/* Start tracking malloc/free events.
 *
 * Returns:
 *   0 = success.
 *   1 = failure.
 */
int memory_leak_tool_start(void)
{
	if (!moduleInitialized) {
		MEM_HOOK_LOGGER("memory_leak_tool has not been initialized.  Please call memory_leak_tool_init().");
		return 1;
	}

	memoryHooksEnabled = 1;
	return 0;
}

/* Stop tracking malloc/free events.
 *
 * Returns:
 *   0 = success.
 *   1 = failure.
 */
int memory_leak_tool_stop(void)
{
	if (!moduleInitialized) {
		MEM_HOOK_LOGGER("memory_leak_tool has not been initialized.  Please call memory_leak_tool_init().");
		return 1;
	}

	memoryHooksEnabled = 0;
	return 0;
}

/* Reset the memory_leak_tool.  This purges all of the tracking data that has
 * been stored internally.  If the memory_leak_tool has been started, then it
 * continues to run.
 *
 * Returns:
 *   0 = success.
 *   1 = failure.
 */
int memory_leak_tool_reset(void)
{
	if (!moduleInitialized) {
		MEM_HOOK_LOGGER("memory_leak_tool has not been initialized.  Please call memory_leak_tool_init().");
		return 1;
	}

	pthread_mutex_lock(&used_event_queue_mutex);
	pthread_mutex_lock(&free_event_queue_mutex);

	int i;
	for (i = 0; i < EVENT_QUEUE_NUM_BUCKETS; i++) {
		struct alloc_event *entry;
		TAILQ_FOREACH(entry, &used_event_queue[i], tailq) {
			TAILQ_REMOVE(&used_event_queue[i], entry, tailq);
			TAILQ_INSERT_TAIL(&free_event_queue, entry, tailq);
		}
		TAILQ_FOREACH(entry, &callers_event_queue[i], tailq) {
			TAILQ_REMOVE(&callers_event_queue[i], entry, tailq);
			TAILQ_INSERT_TAIL(&free_event_queue, entry, tailq);
		}
	}

	alloc_event_adds = 0;
	alloc_event_dels = 0;
	total_bytes_allocated = 0;
	total_bytes_freed = 0;

	pthread_mutex_unlock(&free_event_queue_mutex);
	pthread_mutex_unlock(&used_event_queue_mutex);

	return 0;
}

/* Dump the tracking data that is currently stored inside the memory_leak_tool.
 * The data is dumped to a log file.
 *
 * Returns:
 *   0 = success.
 *   1 = failure.
 */
int memory_leak_tool_log_data(void)
{
	if (!moduleInitialized) {
		MEM_HOOK_LOGGER("memory_leak_tool has not been initialized.  Please call memory_leak_tool_init().");
		return 1;
	}

	processingAnOperation = 1;

	int fd = open(LOG_FILE, O_RDWR | O_CREAT | O_TRUNC, 0777);
	if (fd == -1) {
		MEM_HOOK_LOGGER("Unable to open log file (%s).\n", LOG_FILE);
		return 1;
	}
	dprintf(fd, "%s\n", separator);

	pthread_mutex_lock(&used_event_queue_mutex);
	pthread_mutex_lock(&free_event_queue_mutex);

	dprintf(fd, "alloc_event_adds %d (%ld) : alloc_event_dels %d (%ld) : diff %d (%ld).\n",
		alloc_event_adds, total_bytes_allocated, alloc_event_dels, total_bytes_freed,
		alloc_event_adds - alloc_event_dels, total_bytes_allocated - total_bytes_freed);

	int i;

	// Print the list that is sorted by "caller's PC".
	for (i = 0; i < EVENT_QUEUE_NUM_BUCKETS; i++) {
		struct alloc_event *entry;
		TAILQ_FOREACH(entry, &callers_event_queue[i], tailq) {

			char callerList[NUM_CALLERS * 32];
			memset(callerList, 0, sizeof(callerList));
			int x;
			for (x = 0; (x < NUM_CALLERS) && (x < entry->num_callers); x++) {
				char oneCaller[32];
				sprintf(oneCaller, "%p ", entry->callers[x]);
				strcat(callerList, oneCaller);
			}
			dprintf(fd, "Reference count %4d: Size %7ld: Callers (%ld) ( %s).\n",
				entry->reference_count, entry->size,
				entry->num_callers, callerList);
		}
	}
	dprintf(fd, "%s\n", separator);

	// Print the list that is sorted by "allocated address".
	int num_entries = 0;
	size_t total_bytes_allocated = 0;
	for (i = 0; i < EVENT_QUEUE_NUM_BUCKETS; i++) {
		struct alloc_event *entry;
		TAILQ_FOREACH(entry, &used_event_queue[i], tailq) {

			num_entries++;
			total_bytes_allocated += entry->size;

			char callerList[NUM_CALLERS * 32];
			memset(callerList, 0, sizeof(callerList));
			int x;
			for (x = 0; (x < NUM_CALLERS) && (x < entry->num_callers); x++) {
				char oneCaller[32];
				sprintf(oneCaller, "%p ", entry->callers[x]);
				strcat(callerList, oneCaller);
			}
			dprintf(fd, "Ptr %p: Size %7ld: Callers (%ld) ( %s).\n",
				    entry->ptr, entry->size, entry->num_callers, callerList);
		}
	}
	dprintf(fd, "num_entries %d.  total_bytes_allocated %ld.\n", num_entries, total_bytes_allocated);
	dprintf(fd, "%s\n", separator);

	pthread_mutex_unlock(&free_event_queue_mutex);
	pthread_mutex_unlock(&used_event_queue_mutex);

	struct mallinfo mInfo = mallinfo();
	dprintf(fd, "mallinfo(): arena = %u, ordblks = %u, smblks = %u, hblks = %u, hblkhd = %u, usmblks = %u, fsmblks = %u, uordblks = %u, fordblks = %u, keepcost = %u\n",
	        mInfo.arena, mInfo.ordblks, mInfo.smblks, mInfo.hblks, mInfo.hblkhd,
	        mInfo.usmblks, mInfo.fsmblks, mInfo.uordblks, mInfo.fordblks, mInfo.keepcost);
	dprintf(fd, "%s\n", separator);

	close(fd);

	processingAnOperation = 0;
}

