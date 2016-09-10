#include "config.h"

#include "qemu-common.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "tcg-op.h"

#include "sys/time.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

void init_emscripten_codegen()
{
#ifdef __EMSCRIPTEN__
    EM_ASM({
       CompiledTB = {};
       TBCount = {};
    });
#endif
}


#ifdef __EMSCRIPTEN__

static int tb_count;
static int compiled_tb_count;
static double compiler_time;
static double prev_update;

static double get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + ((double)tv.tv_usec) / 1e6;
}

#define TB_INTERPRET 123
#define TB_COMPILE_PENDING 321

static uint8_t *tb_try_execute(CPUArchState *env, uint8_t *tb_ptr)
{
    return EM_ASM_INT({
        var fun = CompiledTB[$0];
        if(fun !== undefined) {
            return fun($1);
        }
        var tmp = TBCount[$0] | 0;
        TBCount[$0] = tmp + 1;
        return (tmp >= 10) ? 321 : 123;
    }, tb_ptr, env);
}

void update_qemu_stats()
{
    double cur_time = get_time();
    double delta_t = cur_time - prev_update;

    if(delta_t < 1)
        return;

    int tb_per_sec = tb_count / delta_t;
    int compiled_tb_percent = 100.0 * compiled_tb_count / tb_count;
    int compiler_cpu = 100.0 * compiler_time / delta_t;

    tb_count = 0;
    compiled_tb_count = 0;
    compiler_time = 0;
    prev_update = cur_time;

    EM_ASM_ARGS({
        Module.setQemuStats($0, $1, $2);
    }, tb_per_sec, compiled_tb_percent, compiler_cpu);
}


char translation_buf[1000000];

uintptr_t tcg_qemu_tb_exec_real(CPUArchState *env, uint8_t *tb_ptr);

// TODO clean up unused compiled functions, invalidate when required

uintptr_t tcg_qemu_tb_exec(CPUArchState *env, uint8_t *tb_ptr)
{
    tb_count += 1;
    int res = tb_try_execute(env, tb_ptr);
    //fprintf(stderr, "res = %d\n", res);
    if(res == TB_INTERPRET)
    {
        return tcg_qemu_tb_exec_real(env, tb_ptr);
    }

    compiled_tb_count += 1;
    if(res != TB_COMPILE_PENDING)
    {
        return res;
    }

    double t1 = get_time();
    void *ptr = translation_buf;
    translation_buf[0] = 0;
    ptr += sprintf(ptr, "CompiledTB[%d] = function(env) { return Module.dynCall_iii(%d, env, %d); }", tb_ptr, tcg_qemu_tb_exec_real, tb_ptr);
    //fprintf(stderr, "Compiling %p: %s\n", tb_ptr, translation_buf);
    emscripten_run_script(translation_buf);
    compiler_time += get_time() - t1;

    return tb_try_execute(env, tb_ptr);
}
#endif