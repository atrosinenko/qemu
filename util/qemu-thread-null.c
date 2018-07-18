/*
 * Dummy stubs for thread implementation
 *
 * Copyright Anatoly Trosinenko, 2018
 *
 * Based on qemu-thread-posix.c
 * 
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qemu/notify.h"
#include "qemu-thread-common.h"


// TODO What about __thread?

static int current_thread_id = -1;
void qemu_thread_switch(QemuThread *thread) {
    current_thread_id = thread->id;
}

void qemu_thread_naming(bool enable)
{
}

static inline void thread_assert(bool cond, const char *file, int line)
{
#ifndef NDEBUG
    if (!cond) {
        fprintf(stderr, "Single-threading failed at %s:%d\n", file, line);
        abort();
    }
#endif
}

void qemu_mutex_init(QemuMutex *mutex)
{
    mutex->is_rec = 0;
    mutex->lock_counter = 0;
    mutex->owner = -1;
    
    qemu_mutex_post_init(mutex);
}

void qemu_mutex_destroy(QemuMutex *mutex)
{
    assert(mutex->initialized);
    mutex->initialized = false;
}

void qemu_mutex_lock_impl(QemuMutex *mutex, const char *file, const int line)
{
    assert(mutex->initialized);
    qemu_mutex_pre_lock(mutex, file, line);
    thread_assert(
        mutex->lock_counter == 0 || (mutex->is_rec && mutex->owner == current_thread_id),
        file, line
    );
    mutex->lock_counter++;
    mutex->owner = current_thread_id;
    qemu_mutex_post_lock(mutex, file, line);
}

int qemu_mutex_trylock_impl(QemuMutex *mutex, const char *file, const int line)
{
    assert(mutex->initialized);
    if (mutex->lock_counter == 0 || (mutex->is_rec && mutex->owner == current_thread_id)) {
        mutex->lock_counter++;
        mutex->owner = current_thread_id;
        qemu_mutex_post_lock(mutex, file, line);
        return 0;
    }
    return -EBUSY;
}

void qemu_mutex_unlock_impl(QemuMutex *mutex, const char *file, const int line)
{
    assert(mutex->initialized);
    qemu_mutex_pre_unlock(mutex, file, line);
    thread_assert(mutex->lock_counter > 0, file, line);
    mutex->lock_counter--;
}

void qemu_rec_mutex_init(QemuRecMutex *mutex)
{
    mutex->is_rec = 1;
    mutex->lock_counter = 0;
    
    mutex->initialized = true;
}



void qemu_cond_init(QemuCond *cond)
{
    cond->initialized = true;
}

void qemu_cond_destroy(QemuCond *cond)
{
    assert(cond->initialized);
    cond->initialized = false;
}

void qemu_cond_signal(QemuCond *cond)
{
    assert(cond->initialized);
}

void qemu_cond_broadcast(QemuCond *cond)
{
    assert(cond->initialized);
}

void qemu_cond_wait_impl(QemuCond *cond, QemuMutex *mutex, const char *file, const int line)
{
    assert(cond->initialized);
    thread_assert(mutex->lock_counter > 0 && mutex->owner == current_thread_id, file, line);
    thread_assert(0, file, line);
}



#ifdef CONFIG_SEM_TIMEDWAIT
# error Not supported
#endif
void qemu_sem_init(QemuSemaphore *sem, int init)
{
    sem->counter = init;
    sem->initialized = true;
}

void qemu_sem_destroy(QemuSemaphore *sem)
{
    assert(sem->initialized);
    sem->initialized = false;
}

void qemu_sem_post(QemuSemaphore *sem)
{
    assert(sem->initialized);
    sem->counter++;
}

void qemu_sem_wait(QemuSemaphore *sem)
{
    assert(sem->initialized);
    assert(sem->counter > 0);
    sem->counter--;
}

int qemu_sem_timedwait(QemuSemaphore *sem, int ms)
{
    assert(sem->initialized);
    if (sem->counter > 0) {
        sem->counter--;
        return 0;
    }
    return -EBUSY;
}



void qemu_event_init(QemuEvent *ev, bool init)
{
    ev->set = init;
    ev->initialized = true;
}

void qemu_event_destroy(QemuEvent *ev)
{
    assert(ev->initialized);
    ev->initialized = false;
}

void qemu_event_set(QemuEvent *ev)
{
    assert(ev->initialized);
    ev->set = 1;
}

void qemu_event_reset(QemuEvent *ev)
{
    assert(ev->initialized);
    ev->set = 0;
}

void qemu_event_wait(QemuEvent *ev)
{
    assert(ev->initialized);
    assert(ev->set);
}



void qemu_thread_atexit_add(Notifier *notifier)
{
}

void qemu_thread_atexit_remove(Notifier *notifier)
{
}



static int thread_counter = 0;
void qemu_thread_create(QemuThread *thread, const char *name,
                       void *(*start_routine)(void*),
                       void *arg, int mode)
{
    thread->id = thread_counter++;
    fprintf(stderr, "Requested start of thread: %s\n", name);
}

void qemu_thread_get_self(QemuThread *thread)
{
    thread->id = current_thread_id;
}

bool qemu_thread_is_self(QemuThread *thread)
{
   return thread->id == current_thread_id;
}

void qemu_thread_exit(void *retval)
{
    abort();
}

void *qemu_thread_join(QemuThread *thread)
{
    abort();
}
