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
        CompilerFFI = {
            dynCall_iiiiiiiiiii: dynCall_iiiiiiiiiii,
            getTempRet0: Runtime.getTempRet0,
            badAlignment: function() { throw "bad alignment"; },
            _i64Add: Module._i64Add,
            _i64Subtract: Module._i64Subtract,
            _mul_unsigned_long_long: Module._mul_unsigned_long_long,
            Math_imul: Math.imul,
            execute_if_compiled: function(tb_ptr, env, sp_value, depth) {
                var fun = CompiledTB[tb_ptr|0];
                if((depth|0) < 10 && fun != undefined && fun != null)
                    return fun(tb_ptr|0, env|0, sp_value|0, ((depth|0) + 1)|0)|0;
                else
                    return -(tb_ptr|0);
            },
            getThrew: function() { return asm.__THREW__|0; },
            abort: function() { throw "abort"; },
            qemu_ld_ub: Module._helper_ret_ldub_mmu,
            qemu_ld_leuw: Module._helper_le_lduw_mmu,
            qemu_ld_leul: Module._helper_le_ldul_mmu,
            qemu_ld_beuw: Module._helper_be_lduw_mmu,
            qemu_ld_beul: Module._helper_be_ldul_mmu,
            qemu_st_b: Module._helper_ret_stb_mmu,
            qemu_st_lew: Module._helper_le_stw_mmu,
            qemu_st_lel: Module._helper_le_stl_mmu,
            qemu_st_bew: Module._helper_be_stw_mmu,
            qemu_st_bel: Module._helper_be_stl_mmu
        };
    });
#endif
}

int get_tb_start_and_length(int ptr, int *start, int *length);

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

// TODO clean up unused compiled functions, invalidate when required

static uint8_t *tb_try_execute(CPUArchState *env, int tb_key, uint8_t *tb_ptr, uintptr_t sp_value)
{
    return (uint8_t *) EM_ASM_INT({
        var fun = CompiledTB[$1|0];
        if((fun !== undefined) && (fun != null)) {
            return fun($2|0, $0|0, $3|0, 0)|0;
        }
        var tmp = TBCount[$1|0] | 0;
        TBCount[$1|0] = (tmp + 1) | 0;
        return (tmp >= 100) ? 321 : 123;
    }, env, tb_key, tb_ptr, sp_value);
}

void fast_invalidate_tb(int tb_ptr)
{
    EM_ASM_ARGS({
        CompiledTB[$0|0] = null;
    }, tb_ptr);
}

void invalidate_tb(int tb_ptr)
{
    int start, length;
    if(!get_tb_start_and_length(tb_ptr, &start, &length))
        return;
    fast_invalidate_tb(start);
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


#define TRANSLATION_BUF_SIZE 1000000
static char translation_buf[TRANSLATION_BUF_SIZE];

/* Read constant (native size) from bytecode. */
static tcg_target_ulong tci_read_i(uint8_t **tb_ptr)
{
    tcg_target_ulong value = ldl_he_p(*tb_ptr); // TODO: 32bit only, signedness mismatch
    *tb_ptr += sizeof(value);
    return value;
}

/* Read unsigned constant (32 bit) from bytecode. */
static uint32_t tci_read_i32(uint8_t **tb_ptr)
{
    uint32_t value = ldl_he_p(*tb_ptr); // TODO: signedness mismatch
    *tb_ptr += sizeof(value);
    return value;
}

/* Read signed constant (32 bit) from bytecode. */
static int32_t tci_read_s32(uint8_t **tb_ptr)
{
    int32_t value = ldl_he_p(*tb_ptr);
    *tb_ptr += sizeof(value);
    return value;
}

static tcg_target_ulong tci_read_label(uint8_t **tb_ptr)
{
    tcg_target_ulong label = tci_read_i(tb_ptr);
    assert(label != 0);
    return label;
}

void translation_overflow()
{
    fprintf(stderr, "translation buffer overflow\n");
    abort();
}

#define OUT(...) {if(ptr > translation_buf + TRANSLATION_BUF_SIZE - 1000) \
                     translation_overflow(); \
                 ptr += sprintf(ptr, __VA_ARGS__);}

#define WR_REG32(regnum) OUT("    HEAPU32[%d] = ", ((int)(tci_reg + (regnum))) >> 2)

#define RD_REGS32(regnum) OUT("(HEAP32[%d]|0)", ((int)(tci_reg + (regnum)) >> 2))
#define RD_REGS16(regnum) OUT("(HEAP16[%d]|0)", ((int)(tci_reg + (regnum)) >> 1))
#define RD_REGS8 (regnum) OUT("(HEAP8 [%d]|0)", ((int)(tci_reg + (regnum)) >> 0))

#define RD_REGU32(regnum) OUT("(HEAPU32[%d]>>>0)", ((int)(tci_reg + (regnum)) >> 2))
#define RD_REGU16(regnum) OUT("(HEAPU16[%d]>>>0)", ((int)(tci_reg + (regnum)) >> 1))
#define RD_REGU8 (regnum) OUT("(HEAPU8 [%d]>>>0)", ((int)(tci_reg + (regnum)) >> 0))

#define BEFORE_CALL
#define AFTER_CALL OUT("if(getThrew() | 0) abort();\n")

static char *ptr = translation_buf;

extern tcg_target_ulong tci_reg[TCG_TARGET_NB_REGS];

#define TODO() {fprintf(stderr, "Unimplemented opcode: %d\n", opc); abort();}

/* Read indexed register or constant (native size) from bytecode. */
static void tci_gen_ri(uint8_t **tb_ptr)
{
    TCGReg r = **tb_ptr;
    *tb_ptr += 1;
    if (r == TCG_CONST) {
        OUT("(%u>>>0)", tci_read_i(tb_ptr));
    } else {
        RD_REGU32(r);
    }
}

// TODO tci_read_ulong 64-bit guest

static void tci_compare32(TCGCond condition)
{
    switch (condition) {
    case TCG_COND_EQ:
        OUT("    result = ((u0|0) == (u1|0))|0;\n");
        break;
    case TCG_COND_NE:
        OUT("    result = ((u0|0) != (u1|0))|0;\n");
        break;
    case TCG_COND_LT:
        OUT("    result = ((u0|0) < (u1|0))|0;\n");
        break;
    case TCG_COND_GE:
        OUT("    result = ((u0|0) >= (u1|0))|0;\n");
        break;
    case TCG_COND_LE:
        OUT("    result = ((u0|0) <= (u1|0))|0;\n");
        break;
    case TCG_COND_GT:
        OUT("    result = ((u0|0) > (u1|0))|0;\n");
        break;
    case TCG_COND_LTU:
        OUT("    result = ((u0>>>0) < (u1>>>0))|0;\n");
        break;
    case TCG_COND_GEU:
        OUT("    result = ((u0>>>0) >= (u1>>>0))|0;\n");
        break;
    case TCG_COND_LEU:
        OUT("    result = ((u0>>>0) <= (u1>>>0))|0;\n");
        break;
    case TCG_COND_GTU:
        OUT("    result = ((u0>>>0) > (u1>>>0))|0;\n");
        break;
    default:
        abort();
    }
}

unsigned long long mul_unsigned_long_long(unsigned long long x, unsigned long long y)
{
    return x * y;
}

void codegen_main(uint8_t *tb_start, uint8_t *tb_end, uint8_t *tb_ptr, int depth);

// based on tci.c
static void codegen(CPUArchState *env, uint8_t *tb_ptr, int length)
{
    ptr = translation_buf;
    OUT("CompiledTB[0x%08x] = function(stdlib, ffi, heap) {\n", (unsigned int)tb_ptr);
    OUT("\"use asm\";\n");
    OUT("var HEAP8 = new stdlib.Int8Array(heap);\n");
    OUT("var HEAP16 = new stdlib.Int16Array(heap);\n");
    OUT("var HEAP32 = new stdlib.Int32Array(heap);\n");
    OUT("var HEAPU8 = new stdlib.Uint8Array(heap);\n");
    OUT("var HEAPU16 = new stdlib.Uint16Array(heap);\n");
    OUT("var HEAPU32 = new stdlib.Uint32Array(heap);\n");
    OUT("\n");

    OUT("var dynCall_iiiiiiiiiii = ffi.dynCall_iiiiiiiiiii;\n");
    OUT("var getTempRet0 = ffi.getTempRet0;\n");
    OUT("var badAlignment = ffi.badAlignment;\n");
    OUT("var _i64Add = ffi._i64Add;\n");
    OUT("var _i64Subtract = ffi._i64Subtract;\n");
    OUT("var Math_imul = ffi.Math_imul;\n");
    OUT("var _mul_unsigned_long_long = ffi._mul_unsigned_long_long;\n");
    OUT("var execute_if_compiled = ffi.execute_if_compiled;\n");
    OUT("var getThrew = ffi.getThrew;\n");
    OUT("var abort = ffi.abort;\n");
    OUT("var qemu_ld_ub = ffi.qemu_ld_ub;\n");
    OUT("var qemu_ld_leuw = ffi.qemu_ld_leuw;\n");
    OUT("var qemu_ld_leul = ffi.qemu_ld_leul;\n");
    OUT("var qemu_ld_beuw = ffi.qemu_ld_beuw;\n");
    OUT("var qemu_ld_beul = ffi.qemu_ld_beul;\n");
    OUT("var qemu_st_b = ffi.qemu_st_b;\n");
    OUT("var qemu_st_lew = ffi.qemu_st_lew;\n");
    OUT("var qemu_st_lel = ffi.qemu_st_lel;\n");
    OUT("var qemu_st_bew = ffi.qemu_st_bew;\n");
    OUT("var qemu_st_bel = ffi.qemu_st_bel;\n");

    OUT("\n");
    OUT("function tb_fun(tb_ptr, env, sp_value, depth) {\n");
    OUT("  tb_ptr = tb_ptr|0;\n");
    OUT("  env = env|0;\n");
    OUT("  sp_value = sp_value|0;\n");
    OUT("  depth = depth|0;\n");
    OUT("  var u0 = 0, u1 = 0, u2 = 0, u3 = 0, result = 0;\n");
    WR_REG32(TCG_AREG0); OUT("env|0;\n");
    WR_REG32(TCG_REG_CALL_STACK); OUT("sp_value|0;\n");

    OUT("  START: do {\n");
    codegen_main(tb_ptr, tb_ptr + length, tb_ptr, 0);
    OUT("    break;\n");
    OUT("  } while(1); abort(); return 0|0;\n");
    
    OUT("}\n");
    OUT("return {tb_fun: tb_fun};\n");
    OUT("}(window, CompilerFFI, Module.buffer)[\"tb_fun\"]\n");
}

void codegen_main(uint8_t *tb_start, uint8_t *tb_end, uint8_t *tb_ptr, int depth)
{
    if(depth > 30)
        abort();
    for(;;) {
        if(!tb_ptr) {
            fprintf(stderr, "tb_ptr == NULL\n");
            OUT("abort();\n");
            return;
        }

        if(tb_ptr < tb_start || tb_ptr >= tb_end)
        {
            OUT("    return execute_if_compiled(%u|0, env|0, sp_value|0, depth|0) | 0;\n", (unsigned int)tb_ptr);
            return;
        }
        TCGOpcode opc = tb_ptr[0];
#if !defined(NDEBUG)
        uint8_t op_size = tb_ptr[1];
        uint8_t *old_code_ptr = tb_ptr;
#endif
        unsigned int t0;
        unsigned int t1;
        unsigned int label;
        TCGCond condition;
        TCGMemOpIdx oi;

        int start, length;
        /* Skip opcode and size entry. */
        tb_ptr += 2;
        switch (opc) {
        case INDEX_op_call:
            // TODO ASYNC
            BEFORE_CALL;
            WR_REG32(TCG_REG_R0); OUT("dynCall_iiiiiiiiiii((");
            tci_gen_ri(&tb_ptr); OUT(")|0, (");
            RD_REGU32(0); OUT(")|0, (");
            RD_REGU32(1); OUT(")|0, (");
            RD_REGU32(2); OUT(")|0, (");
            RD_REGU32(3); OUT(")|0, (");
            // No reg #4 !!!
            RD_REGU32(5); OUT(")|0, (");
            RD_REGU32(6); OUT(")|0, (");
            RD_REGU32(7); OUT(")|0, (");
            RD_REGU32(8); OUT(")|0, (");
            RD_REGU32(9); OUT(")|0, (");
            RD_REGU32(10); OUT(")|0)|0;\n");
            AFTER_CALL;
            WR_REG32(TCG_REG_R1); OUT("getTempRet0()|0;\n");
            break;
        case INDEX_op_br:
            label = tci_read_label(&tb_ptr);
            assert(tb_ptr == old_code_ptr + op_size);
            tb_ptr = (uint8_t *) label;
            if(tb_ptr == tb_start)
            {
                OUT("    continue START;\n");
                return;
            }
            continue;
        case INDEX_op_setcond_i32:
            t0 = *tb_ptr++;
            OUT("    u0 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            OUT("    u1 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            condition = *tb_ptr++;
            tci_compare32(condition);
            WR_REG32(t0); OUT("result|0;\n");
            break;
        case INDEX_op_setcond2_i32:
            TODO();
//            t0 = *tb_ptr++;
//            tmp64 = tci_read_r64(&tb_ptr);
//            v64 = tci_read_ri64(&tb_ptr);
//            condition = *tb_ptr++;
//            tci_write_reg32(t0, tci_compare64(tmp64, v64, condition));
            break;
        case INDEX_op_mov_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0);
            tci_gen_ri(&tb_ptr);
            OUT(";\n");
            break;
        case INDEX_op_movi_i32:
            t0 = *tb_ptr++;
            t1 = tci_read_i32(&tb_ptr);
            WR_REG32(t0); OUT("%u;\n", t1);
            break;

            /* Load/store operations (32 bit). */

        case INDEX_op_ld8u_i32:
            t0 = *tb_ptr++;
            OUT("    u0 = (");
            tci_gen_ri(&tb_ptr);
            OUT(") + (%d))|0;\n", tci_read_s32(&tb_ptr));
            WR_REG32(t0); OUT("HEAPU8[u0]&255;\n");
            break;
        case INDEX_op_ld8s_i32:
        case INDEX_op_ld16u_i32:
            TODO();
            break;
        case INDEX_op_ld16s_i32:
            TODO();
            break;
        case INDEX_op_ld_i32:
            t0 = *tb_ptr++;
            // TODO check alignment
            OUT("    u0 = (");
            tci_gen_ri(&tb_ptr);
            OUT(" + (%d))|0;\n", (int)tci_read_s32(&tb_ptr));
//             OUT("    if(((u0|0) %% 4) | 0 != 0) badAlignment();\n");
            WR_REG32(t0); OUT("HEAPU32[(u0|0) >> 2]>>>0;\n");
            break;
        case INDEX_op_st8_i32:
            OUT("    u0 = (");
            tci_gen_ri(&tb_ptr);
            OUT(">>>0) & 255;\n");
            OUT("    u1 = (");
            tci_gen_ri(&tb_ptr);
            OUT(" + (%d))|0;\n", (int)tci_read_s32(&tb_ptr));
            OUT("    HEAPU8[u1|0] = u0>>>0;\n");
            break;
        case INDEX_op_st16_i32:
            OUT("    u0 = (");
            tci_gen_ri(&tb_ptr);
            OUT(">>>0) & 65535;\n");
            OUT("    u1 = (");
            tci_gen_ri(&tb_ptr);
            OUT(" + (%d))|0;\n", (int)tci_read_s32(&tb_ptr));
//             OUT("    if(((u1|0) %% 2) | 0 != 0) badAlignment();\n");
            OUT("    HEAPU16[(u1|0) >> 1] = u0>>>0;\n");
            break;
        case INDEX_op_st_i32:
            OUT("    u0 = (");
            tci_gen_ri(&tb_ptr);
            OUT(">>>0);\n");
            OUT("    u1 = (");
            tci_gen_ri(&tb_ptr);
            OUT(" + (%d))|0;\n", (int)tci_read_s32(&tb_ptr));
//             OUT("    if(((u1|0) %% 4) | 0 != 0) badAlignment();\n");
            OUT("    HEAPU32[(u1|0) >> 2] = u0>>>0;\n");
            break;

            /* Arithmetic operations (32 bit). */

        case INDEX_op_add_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT(") + (");
            tci_gen_ri(&tb_ptr);
            OUT("))|0;\n");
            break;
        case INDEX_op_sub_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT(") - (");
            tci_gen_ri(&tb_ptr);
            OUT("))|0;\n");
            break;
        case INDEX_op_mul_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0);
            OUT("Math_imul((");
            tci_gen_ri(&tb_ptr);
            OUT(")|0, (");
            tci_gen_ri(&tb_ptr);
            OUT(")|0)|0;\n");
            break;
#if TCG_TARGET_HAS_div_i32
        case INDEX_op_div_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT("|0) / (");
            tci_gen_ri(&tb_ptr);
            OUT("|0))&-1;\n");
            break;
        case INDEX_op_divu_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT(">>>0) / (");
            tci_gen_ri(&tb_ptr);
            OUT(">>>0))&-1;\n");
            break;
        case INDEX_op_rem_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT("|0) %% (");
            tci_gen_ri(&tb_ptr);
            OUT("|0))&-1;\n");
            break;
        case INDEX_op_remu_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT(">>>0) %% (");
            tci_gen_ri(&tb_ptr);
            OUT(">>>0))&-1;\n");
            break;
#elif TCG_TARGET_HAS_div2_i32
        case INDEX_op_div2_i32:
        case INDEX_op_divu2_i32:
            TODO();
            break;
#endif
        case INDEX_op_and_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT(") & (");
            tci_gen_ri(&tb_ptr);
            OUT("))|0;\n");
            break;
        case INDEX_op_or_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT(") | (");
            tci_gen_ri(&tb_ptr);
            OUT("))|0;\n");
            break;
        case INDEX_op_xor_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT(") ^ (");
            tci_gen_ri(&tb_ptr);
            OUT("))|0;\n");
            break;

            /* Shift/rotate operations (32 bit). */

        case INDEX_op_shl_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT(") << ((");
            tci_gen_ri(&tb_ptr);
            OUT(">>>0) & 31))|0;\n");
            break;
        case INDEX_op_shr_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT(") >>> ((");
            tci_gen_ri(&tb_ptr);
            OUT(">>>0) & 31))|0;\n");
            break;
        case INDEX_op_sar_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT(") >> ((");
            tci_gen_ri(&tb_ptr);
            OUT(">>>0) & 31))|0;\n");
            break;
#if 0 //TCG_TARGET_HAS_rot_i32
        case INDEX_op_rotl_i32:
            t0 = *tb_ptr++;
            t1 = tci_read_ri32(&tb_ptr);
            t2 = tci_read_ri32(&tb_ptr);
            tci_write_reg32(t0, rol32(t1, t2 & 31));
            break;
        case INDEX_op_rotr_i32:
            t0 = *tb_ptr++;
            t1 = tci_read_ri32(&tb_ptr);
            t2 = tci_read_ri32(&tb_ptr);
            tci_write_reg32(t0, ror32(t1, t2 & 31));
            break;
#endif
#if 0 // TCG_TARGET_HAS_deposit_i32
        case INDEX_op_deposit_i32:
            t0 = *tb_ptr++;
            t1 = tci_read_r32(&tb_ptr);
            t2 = tci_read_r32(&tb_ptr);
            tmp16 = *tb_ptr++;
            tmp8 = *tb_ptr++;
            tmp32 = (((1 << tmp8) - 1) << tmp16);
            tci_write_reg32(t0, (t1 & ~tmp32) | ((t2 << tmp16) & tmp32));
            break;
#endif
        case INDEX_op_brcond_i32:
            OUT("    u0 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            OUT("    u1 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            condition = *tb_ptr++;
            label = tci_read_label(&tb_ptr);
            tci_compare32(condition);
            OUT("    if(result|0) {\n");
            if(label == tb_start)
            {
                OUT("    continue START;\n");
            }
            else
            {
                codegen_main(tb_start, tb_end, (uint8_t *) label, depth + 1);
            }
            OUT("    }\n");
            break;
        case INDEX_op_add2_i32:
            t0 = *tb_ptr++;
            t1 = *tb_ptr++;
            OUT("    u0 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            OUT("    u1 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            OUT("    u2 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            OUT("    u3 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            WR_REG32(t0); OUT("_i64Add(u0|0, u1|0, u2|0, u3|0)|0;\n");
            WR_REG32(t1); OUT("getTempRet0()|0;\n");
            break;
        case INDEX_op_sub2_i32:
            t0 = *tb_ptr++;
            t1 = *tb_ptr++;
            OUT("    u0 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            OUT("    u1 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            OUT("    u2 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            OUT("    u3 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            WR_REG32(t0); OUT("_i64Subtract(u0|0, u1|0, u2|0, u3|0)|0;\n");
            WR_REG32(t1); OUT("getTempRet0()|0;\n");
            break;
        case INDEX_op_brcond2_i32:
            TODO();
//            tmp64 = tci_read_r64(&tb_ptr);
//            v64 = tci_read_ri64(&tb_ptr);
//            condition = *tb_ptr++;
//            label = tci_read_label(&tb_ptr);
//            if (tci_compare64(tmp64, v64, condition)) {
//                assert(tb_ptr == old_code_ptr + op_size);
//                tb_ptr = (uint8_t *)label;
//                continue;
//            }
            break;
        case INDEX_op_mulu2_i32:
            t0 = *tb_ptr++;
            t1 = *tb_ptr++;
            OUT("    u0 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            OUT("    u1 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            WR_REG32(t0); OUT("_mul_unsigned_long_long(u0|0, 0, u1|0, 0)|0;\n");
            WR_REG32(t1); OUT("getTempRet0()|0;\n");
            break;
#if TCG_TARGET_HAS_ext8s_i32
        case INDEX_op_ext8s_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("(");
            tci_gen_ri(&tb_ptr);
            OUT("|0) << 24 >> 24;");
            break;
#endif
#if TCG_TARGET_HAS_ext16s_i32
        case INDEX_op_ext16s_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("(");
            tci_gen_ri(&tb_ptr);
            OUT("|0) << 16 >> 16;");
            break;
#endif
#if TCG_TARGET_HAS_ext8u_i32
        case INDEX_op_ext8u_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("(");
            tci_gen_ri(&tb_ptr);
            OUT(") & 255;");
            break;
#endif
#if TCG_TARGET_HAS_ext16u_i32
        case INDEX_op_ext16u_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("(");
            tci_gen_ri(&tb_ptr);
            OUT(") & 65535;");
            break;
#endif
#if 0 // TCG_TARGET_HAS_bswap16_i32
        case INDEX_op_bswap16_i32:
            t0 = *tb_ptr++;
            t1 = tci_read_r16(&tb_ptr);
            tci_write_reg32(t0, bswap16(t1));
            break;
#endif
#if 0 // TCG_TARGET_HAS_bswap32_i32
        case INDEX_op_bswap32_i32:
            t0 = *tb_ptr++;
            t1 = tci_read_r32(&tb_ptr);
            tci_write_reg32(t0, bswap32(t1));
            break;
#endif
#if TCG_TARGET_HAS_not_i32
        case INDEX_op_not_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("((");
            tci_gen_ri(&tb_ptr);
            OUT("|0)^-1)|0;\n");
            break;
#endif
#if TCG_TARGET_HAS_neg_i32
        case INDEX_op_neg_i32:
            t0 = *tb_ptr++;
            WR_REG32(t0); OUT("(0 - (");
            tci_gen_ri(&tb_ptr);
            OUT("))|0;\n");
            break;
#endif

            /* QEMU specific operations. */

        case INDEX_op_debug_insn_start:
            TODO();
            break;
        case INDEX_op_exit_tb:
            OUT("    return 0x%08x|0;\n", (unsigned int)ldq_he_p(tb_ptr));
            return;
        case INDEX_op_goto_tb:
            t0 = tci_read_i32(&tb_ptr);
            assert(tb_ptr == old_code_ptr + op_size);
            tb_ptr += (int32_t)t0;
            if(tb_ptr == tb_start)
            {
                OUT("    continue START;\n");
                return;
            }
            continue;
        case INDEX_op_qemu_ld_i32:
            t0 = *tb_ptr++;
            OUT("    u0 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            oi = tci_read_i(&tb_ptr);
            // TODO check width and signedness
            
            BEFORE_CALL;
            // note tb_ptr in JS is not always updated
            WR_REG32(t0); 
            switch (get_memop(oi) & (MO_BSWAP | MO_SSIZE)) {
            case MO_UB:
                OUT("(qemu_ld_ub(env|0, u0|0, %u, %u) | 0) & 255;\n", oi, (unsigned int)tb_ptr);
                break;
            case MO_SB:
                OUT("(qemu_ld_ub(env|0, u0|0, %u, %u) | 0) << 24 >> 24;\n", oi, (unsigned int)tb_ptr);
                break;
            case MO_LEUW:
                OUT("(qemu_ld_leuw(env|0, u0|0, %u, %u) | 0) & 65535;\n", oi, (unsigned int)tb_ptr);
                break;
            case MO_LESW:
                OUT("(qemu_ld_leuw(env|0, u0|0, %u, %u) | 0) << 16 >> 16;\n", oi, (unsigned int)tb_ptr);
                break;
            case MO_LEUL:
                OUT("qemu_ld_leul(env|0, u0|0, %u, %u)|0;\n", oi, (unsigned int)tb_ptr);
                break;
            case MO_BEUW:
                OUT("(qemu_ld_beuw(env|0, u0|0, %u, %u) | 0) & 65535;\n", oi, (unsigned int)tb_ptr);
                break;
            case MO_BESW:
                OUT("(qemu_ld_beuw(env|0, u0|0, %u, %u) | 0) << 16 >> 16;\n", oi, (unsigned int)tb_ptr);
                break;
            case MO_BEUL:
                OUT("qemu_ld_beul(env|0, u0|0, %u, %u)|0;\n", oi, (unsigned int)tb_ptr);
                break;
            default:
                tcg_abort();
            }
            AFTER_CALL;
            break;
        case INDEX_op_qemu_ld_i64:
            TODO();
/*            t0 = *tb_ptr++;
            if (TCG_TARGET_REG_BITS == 32) {
                t1 = *tb_ptr++;
            }
            taddr = tci_read_ulong(&tb_ptr);
            oi = tci_read_i(&tb_ptr);
            switch (get_memop(oi) & (MO_BSWAP | MO_SSIZE)) {
            case MO_UB:
                tmp64 = qemu_ld_ub;
                break;
            case MO_SB:
                tmp64 = (int8_t)qemu_ld_ub;
                break;
            case MO_LEUW:
                tmp64 = qemu_ld_leuw;
                break;
            case MO_LESW:
                tmp64 = (int16_t)qemu_ld_leuw;
                break;
            case MO_LEUL:
                tmp64 = qemu_ld_leul;
                break;
            case MO_LESL:
                tmp64 = (int32_t)qemu_ld_leul;
                break;
            case MO_LEQ:
                tmp64 = qemu_ld_leq;
                break;
            case MO_BEUW:
                tmp64 = qemu_ld_beuw;
                break;
            case MO_BESW:
                tmp64 = (int16_t)qemu_ld_beuw;
                break;
            case MO_BEUL:
                tmp64 = qemu_ld_beul;
                break;
            case MO_BESL:
                tmp64 = (int32_t)qemu_ld_beul;
                break;
            case MO_BEQ:
                tmp64 = qemu_ld_beq;
                break;
            default:
                tcg_abort();
            }
            tci_write_reg(t0, tmp64);
            if (TCG_TARGET_REG_BITS == 32) {
                tci_write_reg(t1, tmp64 >> 32);
            }
*/            break;
        case INDEX_op_qemu_st_i32:
            OUT("    u0 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            OUT("    u1 = "); tci_gen_ri(&tb_ptr); OUT(";\n");
            oi = tci_read_i(&tb_ptr);
            BEFORE_CALL;
            switch (get_memop(oi) & (MO_BSWAP | MO_SIZE)) {
            case MO_UB:
                OUT("    qemu_st_b(env|0, u1|0, (u0 & 255)|0, %u, %u);\n", oi, (unsigned int)tb_ptr);
                break;
            case MO_LEUW:
                OUT("    qemu_st_lew(env|0, u1|0, (u0 & 65535)|0, %u, %u);\n", oi, (unsigned int)tb_ptr);
                break;
            case MO_LEUL:
                OUT("    qemu_st_lel(env|0, u1|0, u0|0, %u, %u);\n", oi, (unsigned int)tb_ptr);
                break;
            case MO_BEUW:
                OUT("    qemu_st_bew(env|0, u1|0, (u0 & 65535)|0, %u, %u);\n", oi, (unsigned int)tb_ptr);
                break;
            case MO_BEUL:
                OUT("    qemu_st_bel(env|0, u1|0, u0|0, %u, %u);\n", oi, (unsigned int)tb_ptr);
                break;
            default:
                tcg_abort();
            }
            AFTER_CALL;
            break;
        case INDEX_op_qemu_st_i64:
            TODO();
/*            tmp64 = tci_read_r64(&tb_ptr);
            taddr = tci_read_ulong(&tb_ptr);
            oi = tci_read_i(&tb_ptr);
            switch (get_memop(oi) & (MO_BSWAP | MO_SIZE)) {
            case MO_UB:
                qemu_st_b(tmp64);
                break;
            case MO_LEUW:
                qemu_st_lew(tmp64);
                break;
            case MO_LEUL:
                qemu_st_lel(tmp64);
                break;
            case MO_LEQ:
                qemu_st_leq(tmp64);
                break;
            case MO_BEUW:
                qemu_st_bew(tmp64);
                break;
            case MO_BEUL:
                qemu_st_bel(tmp64);
                break;
            case MO_BEQ:
                qemu_st_beq(tmp64);
                break;
            default:
                tcg_abort();
            }
            break;
*/        default:
            TODO();
            break;
        }
        assert(tb_ptr == old_code_ptr + op_size);
    }
}

uintptr_t tcg_qemu_tb_exec_real(CPUArchState *env, uint8_t *tb_ptr);

uintptr_t tcg_qemu_tb_exec(CPUArchState *env, uint8_t *tb_ptr)
{
    long tcg_temps[CPU_TEMP_BUF_NLONGS];
    uintptr_t sp_value = (uintptr_t)(tcg_temps + CPU_TEMP_BUF_NLONGS);
    
//    fprintf(stderr, "Exec: %08x\n", tb_ptr);
    
    int start, length;
    
    int res = -(int)tb_ptr;
    do {
        tb_ptr = -res;
        if(!get_tb_start_and_length(tb_ptr, &start, &length))
            abort();
        if(tb_ptr != start)
            fprintf(stderr, "tb_ptr = %08x start = %08x\n", tb_ptr, start);
        tb_count += 1;
        res = tb_try_execute(env, start, tb_ptr, sp_value);
        if(res != TB_INTERPRET)
            compiled_tb_count += 1;
    }
    while(res < 0);
//    fprintf(stderr, "res = %d\n", res);
    if(res == TB_INTERPRET)
    {
        res = tcg_qemu_tb_exec_real(env, tb_ptr);
//        fprintf(stderr, "Return: %08x\n", res);
        return res;
    }

    if(res != TB_COMPILE_PENDING)
    {
//        fprintf(stderr, "Return: %08x\n", res);
        return res;
    }

    double t1 = get_time();
    translation_buf[0] = 0;
    codegen(env, start, length);
//    fprintf(stderr, "Compiling %p:\n%s\n", tb_ptr, translation_buf);
    emscripten_run_script(translation_buf);
    compiler_time += get_time() - t1;

    return tcg_qemu_tb_exec(env, tb_ptr);
//    uintptr_t r = tb_try_execute(env, start, tb_ptr, sp_value);
//    fprintf(stderr, "Return: %08x\n", r);
//    return r;
}
#endif
