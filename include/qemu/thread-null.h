#ifndef QEMU_THREAD_NULL_H
#define QEMU_THREAD_NULL_H

typedef QemuMutex QemuRecMutex;
#define qemu_rec_mutex_destroy qemu_mutex_destroy
#define qemu_rec_mutex_lock qemu_mutex_lock
#define qemu_rec_mutex_trylock qemu_mutex_trylock
#define qemu_rec_mutex_unlock qemu_mutex_unlock

// For kludgy __thread emulation
#define MAX_THREADS 10

struct QemuMutex {
    int is_rec;
    int lock_counter;
    int owner;
#ifdef CONFIG_DEBUG_MUTEX
    const char *file;
    int line;
#endif
    bool initialized;
};

struct QemuCond {
    bool initialized;
};

struct QemuSemaphore {
    int counter;
    bool initialized;
};

struct QemuEvent {
    int set;
    bool initialized;
};

struct QemuThread {
    int id;
};

void qemu_thread_switch_to_main(void);
void qemu_thread_switch(QemuThread *thread);

#endif
