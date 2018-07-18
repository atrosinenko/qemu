#include <pthread.h>

// For ./configure script
// Execute new thread function just now in this thread
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg)
{
	// typedef unsigned long pthread_t;
	*thread = start_routine(arg);
	return 0;
}

int pthread_join(pthread_t thread, void **retval)
{
	*retval = thread;
	return 0;
}
