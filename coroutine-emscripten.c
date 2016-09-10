#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
#include "qemu-common.h"
#include "block/coroutine_int.h"

#include <emscripten.h>

static Coroutine *current;

struct CoroutineEmscripten {
    Coroutine base;
    emscripten_coroutine cr;
};

Coroutine *qemu_coroutine_new(void) {
    struct CoroutineEmscripten *co;
    co = g_malloc0(sizeof(*co));
//    fprintf(stderr, "%s %p\n", __FUNCTION__, co);

    co->cr = 0;
    return &co->base;
}

void qemu_coroutine_delete(Coroutine *co) {
//    fprintf(stderr, "%s %p\n", __FUNCTION__, co);
    g_free(co);
}

CoroutineAction qemu_coroutine_switch(Coroutine *from_, Coroutine *to_,
                                      CoroutineAction action) {
    struct CoroutineEmscripten *to = DO_UPCAST(struct CoroutineEmscripten, base, to_);
//    fprintf(stderr, "%s %s %p %p | %p %p\n", __FUNCTION__, action == COROUTINE_ENTER ? "enter" : "yield", from_, to_, to_->entry, to->cr);
    current = to_;
    switch(action) {
        case COROUTINE_ENTER:
            if(!to->cr)
                to->cr = emscripten_coroutine_create(to_->entry, to_->entry_arg, 0);
            if(emscripten_coroutine_next(to->cr)) {
//                fprintf(stderr, "Yielded %p | %p\n", to, to->cr);
                if(current != from_)
                    abort();
                return COROUTINE_YIELD;
            }
            else {
                to->cr = 0;
                return COROUTINE_TERMINATE;
            }
            break;
        case COROUTINE_YIELD:
            emscripten_yield();
            return COROUTINE_YIELD;
        default:
            abort();
    }
}

// from coroutine-ucontext.c
static __thread struct CoroutineEmscripten leader;
static __thread Coroutine *current;


Coroutine *qemu_coroutine_self(void)
{
//    fprintf(stderr, "%s\n", __FUNCTION__);
    if (!current) {
        current = &leader.base;
    }
    return current;
}

bool qemu_in_coroutine(void)
{
//    fprintf(stderr, "%s %d\n", __FUNCTION__, current && current->caller);
    return current && current->caller;
}
