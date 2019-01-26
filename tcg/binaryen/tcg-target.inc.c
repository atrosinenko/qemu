/*
 * Tiny Code Generator for QEMU - Binaryen backend
 *
 * Copyright (c) 2018 Anatoly Trosinenko
 * Largely based on tcg/{i386,tci}/tcg-target.inc.c
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <binaryen-c.h>
#include <emscripten.h>


#if MAX_OPC_PARAM_IARGS != 6
# error Fix needed, number of supported input arguments changed!
#endif

BinaryenModuleRef MODULE;

static int tcg_target_reg_alloc_order[TCG_TARGET_NB_REGS - 2];
static const int tcg_target_call_iarg_regs[] = {
    TCG_TARGET_NB_REGS - 12,
    TCG_TARGET_NB_REGS - 11,
    TCG_TARGET_NB_REGS - 10,
    TCG_TARGET_NB_REGS - 9,
    TCG_TARGET_NB_REGS - 8,
    TCG_TARGET_NB_REGS - 7,
    TCG_TARGET_NB_REGS - 6,
    TCG_TARGET_NB_REGS - 5,
    TCG_TARGET_NB_REGS - 4,
    TCG_TARGET_NB_REGS - 3,
    TCG_TARGET_NB_REGS - 2,
    TCG_TARGET_NB_REGS - 1,
};

static const int tcg_target_call_oarg_regs[] = {
    TCG_TARGET_NB_REGS - 2,
    TCG_TARGET_NB_REGS - 1
};

#define GOTO_TB_FLAG        (1u << 31)
#define ENTRY_MARKER (-1)

static int get_fptr(TCGContext *s, uintptr_t *tag)
{
  return ((uintptr_t)(tag) - (uintptr_t)(s->code_gen_buffer)) / 4;
}

static inline void tcg_out_expr(TCGContext *s, BinaryenExpressionRef expr, uint32_t or_mask)
{
    assert((((uintptr_t)expr) & 0x3) == 0);
    uintptr_t *ptr = s->code_ptr;
    if (*ptr == ENTRY_MARKER) {
        EM_ASM({ delete CompiledTBTable[$0]; }, get_fptr(s, ptr));
    }
    tcg_out32(s, ((uintptr_t)expr) | or_mask);
}

#define BINARYEN_GENERIC_FUNC_TYPE "helper-func"
#define BINARYEN_GENERIC_FUNC_TYPE_HI "helper-func-hi"
#define BINARYEN_LD_FUNC_TYPE      "load-func"
#define BINARYEN_ST32_FUNC_TYPE    "store32-func"
#define BINARYEN_ST64_FUNC_TYPE    "store64-func"
#define BINARYEN_TB_FUNC_TYPE      "tb-func"

#define STORE32(n, expr) tcg_out_expr(s, BinaryenSetLocal(MODULE, (n), (expr)), 0)

#define RI32(const_, arg) ((const_) ? \
                            BinaryenConst(MODULE, BinaryenLiteralInt32(arg)) : \
                            BinaryenGetLocal(MODULE, (arg), BinaryenTypeInt32()))


#define ARG32(argn) RI32(const_args[(argn)], args[(argn)])

#define REG32(regn)  BinaryenGetLocal(MODULE, (regn), BinaryenTypeInt32())
#define CONST32(arg) BinaryenConst(MODULE, BinaryenLiteralInt32(arg))
#define CONST64(arg) BinaryenConst(MODULE, BinaryenLiteralInt64(arg))

#define RI64_2reg(const_low, arg_low, const_high, arg_high) \
    BinaryenBinary(MODULE, BinaryenOrInt64(), \
                   BinaryenBinary(MODULE, BinaryenShlInt64(), BinaryenUnary(MODULE, BinaryenExtendUInt32(), RI32(const_high, arg_high)), CONST32(32)), \
                   BinaryenUnary(MODULE, BinaryenExtendUInt32(), RI32(const_low, arg_low)) \
                  )

#define UNARY32( op, to, arg)        STORE32((to), BinaryenUnary (MODULE, (op), (arg)))
#define BINARY32(op, to, arg1, arg2) STORE32((to), BinaryenBinary(MODULE, (op), (arg1), (arg2)))

#define BINARY64(op, to_low, to_high, arg1, arg2) \
    tcg_out_expr(s, BinaryenSetLocal(MODULE, TMP64, BinaryenBinary(MODULE, (op), (arg1), (arg2))), 0); \
    STORE32((to_low),  BinaryenUnary(MODULE, BinaryenWrapInt64(), BinaryenGetLocal(MODULE, TMP64, BinaryenTypeInt64()))); \
    STORE32((to_high), BinaryenUnary(MODULE, BinaryenWrapInt64(), BinaryenBinary(MODULE, BinaryenShrUInt64(), BinaryenGetLocal(MODULE, TMP64, BinaryenTypeInt64()), CONST64(32))));

static BinaryenType int32_helper_args[ARRAY_SIZE(tcg_target_call_iarg_regs) + 1];
static BinaryenType func_locals[TCG_TARGET_NB_REGS + 2]; // 2 x 32bit arg-regs + (NB_REGS - 2) x 32 bit + 3 x (32 bit TLB) + 1 x (64 bit TMP64)

static BinaryenFunctionTypeRef helper_type, ld_type, st32_type, st64_type, tb_func_type, get_temp_ret_type;

static long tcg_temps[CPU_TEMP_BUF_NLONGS];

#define TLB_TMP0 (TCG_TARGET_NB_REGS)
#define TLB_TMP1 (TCG_TARGET_NB_REGS + 1)
#define TLB_TMP2 (TCG_TARGET_NB_REGS + 2)
#define TMP64    (TCG_TARGET_NB_REGS + 3)

void binaryen_module_init(TCGContext *s)
{
    MODULE = BinaryenModuleCreate();
    for (int i = 0; i < ARRAY_SIZE(tcg_target_reg_alloc_order); ++i) {
        tcg_target_reg_alloc_order[i] = i + 2;
    }
    for (int i = 0; i < ARRAY_SIZE(int32_helper_args); ++i) {
        int32_helper_args[i] = BinaryenTypeInt32();
    }
    for (int i = 0; i < ARRAY_SIZE(func_locals) - 1; ++i) {
        func_locals[i] = BinaryenTypeInt32();
    }
    func_locals[ARRAY_SIZE(func_locals) - 1] = BinaryenTypeInt64();
    helper_type =       BinaryenAddFunctionType(MODULE, BINARYEN_GENERIC_FUNC_TYPE,    BinaryenTypeInt32(), int32_helper_args, ARRAY_SIZE(int32_helper_args));
    get_temp_ret_type = BinaryenAddFunctionType(MODULE, BINARYEN_GENERIC_FUNC_TYPE_HI, BinaryenTypeInt32(), NULL, 0);

    ld_type =    BinaryenAddFunctionType(MODULE, BINARYEN_LD_FUNC_TYPE, BinaryenTypeInt32(), int32_helper_args, 4);
    st32_type =    BinaryenAddFunctionType(MODULE, BINARYEN_ST32_FUNC_TYPE, BinaryenTypeNone(), int32_helper_args, 5);
    st64_type =    BinaryenAddFunctionType(MODULE, BINARYEN_ST64_FUNC_TYPE, BinaryenTypeNone(), int32_helper_args, 6);

    tb_func_type = BinaryenAddFunctionType(MODULE, BINARYEN_TB_FUNC_TYPE, BinaryenTypeInt32(), int32_helper_args, 2);
}

uintptr_t (*invoke_tb)(int, void *, void *);
typedef uint64_t (*helper_func)(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5, uint32_t arg6, uint32_t arg7, uint32_t arg8, uint32_t arg9, uint32_t arg10, uint32_t arg11, uint32_t arg12);
uint64_t call_helper(helper_func func, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5, uint32_t arg6, uint32_t arg7, uint32_t arg8, uint32_t arg9, uint32_t arg10, uint32_t arg11, uint32_t arg12)
{
  return func(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12);
}

static void compile_module(TCGContext *s, BinaryenModuleRef module, uintptr_t *tag, BinaryenExpressionRef expr)
{
    static char buf[1 << 20];
    BinaryenAddFunction(MODULE, "tb_fun", tb_func_type, func_locals, ARRAY_SIZE(func_locals), expr);
    BinaryenAddFunctionExport(MODULE, "tb_fun", "tb_fun");

    BinaryenAddFunctionImport(MODULE, "call_helper",  "env", "call_helper", helper_type);

    BinaryenAddFunctionImport(MODULE, "helper_ret_ldub_mmu", "env", "helper_ret_ldub_mmu", ld_type);
    BinaryenAddFunctionImport(MODULE, "helper_le_lduw_mmu" , "env", "helper_le_lduw_mmu" , ld_type);
    BinaryenAddFunctionImport(MODULE, "helper_le_ldul_mmu" , "env", "helper_le_ldul_mmu" , ld_type);
    BinaryenAddFunctionImport(MODULE, "helper_le_ldq_mmu"  , "env", "helper_le_ldq_mmu"  , ld_type);
    BinaryenAddFunctionImport(MODULE, "helper_be_lduw_mmu" , "env", "helper_be_lduw_mmu" , ld_type);
    BinaryenAddFunctionImport(MODULE, "helper_be_ldul_mmu" , "env", "helper_be_ldul_mmu" , ld_type);
    BinaryenAddFunctionImport(MODULE, "helper_be_ldq_mmu"  , "env", "helper_be_ldq_mmu"  , ld_type);

    BinaryenAddFunctionImport(MODULE, "get_temp_ret", "env", "get_temp_ret",get_temp_ret_type);

    BinaryenAddFunctionImport(MODULE, "helper_ret_stb_mmu", "env", "helper_ret_stb_mmu", st32_type);
    BinaryenAddFunctionImport(MODULE, "helper_le_stw_mmu", "env", "helper_le_stw_mmu"  , st32_type);
    BinaryenAddFunctionImport(MODULE, "helper_le_stl_mmu", "env", "helper_le_stl_mmu"  , st32_type);
    BinaryenAddFunctionImport(MODULE, "helper_le_stq_mmu", "env", "helper_le_stq_mmu"  , st32_type);
    BinaryenAddFunctionImport(MODULE, "helper_be_stw_mmu", "env", "helper_be_stw_mmu"  , st32_type);
    BinaryenAddFunctionImport(MODULE, "helper_be_stl_mmu", "env", "helper_be_stl_mmu"  , st32_type);
    BinaryenAddFunctionImport(MODULE, "helper_be_stq_mmu", "env", "helper_be_stq_mmu"  , st64_type);

    BinaryenSetMemory(MODULE, 0, -1, NULL, NULL, NULL, NULL, 0, 0);
    BinaryenAddMemoryImport(MODULE, NULL, "env", "memory", 0);
    BinaryenAddTableImport(MODULE, NULL, "env", "tb_funcs");


//     assert (BinaryenModuleValidate(MODULE));
    BinaryenModuleOptimize(MODULE);
    int sz = BinaryenModuleWrite(MODULE, buf, sizeof(buf));
    BinaryenModuleDispose(MODULE);
    MODULE = NULL;

    EM_ASM({
        var module = new WebAssembly.Module(new Uint8Array(Module['wasmMemory'].buffer, $1, $2));
        var fptr = $3;
        var instance = new WebAssembly.Instance(module, {
            'env': {
                'memory': Module['wasmMemory'],
                'tb_funcs': CompiledTBTable,
                'call_helper': Module['_call_helper'],

                'helper_ret_ldub_mmu': Module['_helper_ret_ldub_mmu'],
                'helper_le_lduw_mmu':  Module['_helper_le_lduw_mmu'],
                'helper_le_ldul_mmu':  Module['_helper_le_ldul_mmu'],
                'helper_le_ldq_mmu':   Module['_helper_le_ldq_mmu'],
                'helper_be_lduw_mmu':  Module['_helper_be_lduw_mmu'],
                'helper_be_ldul_mmu':  Module['_helper_be_ldul_mmu'],
                'helper_be_ldq_mmu':   Module['_helper_be_ldq_mmu'],

                'helper_ret_stb_mmu': Module['_helper_ret_stb_mmu'],
                'helper_le_stw_mmu':  Module['_helper_le_stw_mmu'],
                'helper_le_stl_mmu':  Module['_helper_le_stl_mmu'],
                'helper_le_stq_mmu':  Module['_helper_le_stq_mmu'],
                'helper_be_stw_mmu':  Module['_helper_be_stw_mmu'],
                'helper_be_stl_mmu':  Module['_helper_be_stl_mmu'],
                'helper_be_stq_mmu':  Module['_helper_be_stq_mmu'],

                'get_temp_ret': getTempRet0,
            }
        });
        if (CompiledTBTable.length < fptr + 1) {
          CompiledTBTable.grow(fptr + 10 - CompiledTBTable.length);
        }
        CompiledTBTable.set(fptr, instance.exports['tb_fun']);
    }, tag, buf, sz, get_fptr(s, tag));
    *tag = ENTRY_MARKER;
}

uintptr_t tcg_qemu_tb_exec(CPUArchState *env, uint8_t *_tb_ptr)
{
    long tcg_temps[CPU_TEMP_BUF_NLONGS];
    uintptr_t sp_value = (uintptr_t)(tcg_temps + CPU_TEMP_BUF_NLONGS);

    TCGContext *s = ((uint32_t *)_tb_ptr)[1];
    uintptr_t tb_ptr = _tb_ptr;

    return invoke_tb(get_fptr(s, _tb_ptr), env, sp_value);
}

static void create_invoker()
{
  BinaryenModuleRef invoker = BinaryenModuleCreate();

  // Initialize types
  BinaryenType many_int32s[] = {
    BinaryenTypeInt32(),
    BinaryenTypeInt32(),
    BinaryenTypeInt32()
  };
  BinaryenFunctionTypeRef tb_type = BinaryenAddFunctionType(
    invoker, "tb_type",
    BinaryenTypeInt32(), many_int32s, 2
  );
  BinaryenFunctionTypeRef invoker_type = BinaryenAddFunctionType(
    invoker, "invoker_type",
    BinaryenTypeInt32(), many_int32s, 3
  );

  // Create invoker function
  BinaryenExpressionRef args[] = {
    BinaryenGetLocal(invoker, 1, BinaryenTypeInt32()),
    BinaryenGetLocal(invoker, 2, BinaryenTypeInt32())
  };
  BinaryenExpressionRef function_body = BinaryenReturn(
    invoker,
    BinaryenCallIndirect(
      invoker,
      BinaryenGetLocal(invoker, 0, BinaryenTypeInt32()),
      args, 2, "tb_type"
    )
  );
  BinaryenFunctionRef invoker_tb = BinaryenAddFunction(
    invoker,
    "invoke_tb", invoker_type,
    NULL, 0, function_body
  );

  BinaryenAddTableImport(invoker, NULL, "env", "tb_funcs");
  BinaryenAddFunctionExport(invoker, "invoke_tb", "invoke_tb");


  static char buf[4096];
//  BinaryenModuleValidate(invoker);
  BinaryenModuleOptimize(invoker);
  int sz = BinaryenModuleWrite(invoker, buf, sizeof(buf));
  BinaryenModuleDispose(invoker);

  invoke_tb = EM_ASM_INT({
    var module = new WebAssembly.Module(new Uint8Array(Module['wasmMemory'].buffer, $0, $1));
    window.CompiledTBTable = new WebAssembly.Table({
      'initial': 1024 * 1024 / 4,
      'element': 'anyfunc'
    });
    var instance = new WebAssembly.Instance(module, {
      'env': {
        'tb_funcs': CompiledTBTable
      }
    });
    return Module.addFunction(instance.exports['invoke_tb']);
  }, buf, sz);
}

static void tcg_target_init(TCGContext *s)
{
    BinaryenSetAPITracing(0);

    BinaryenSetOptimizeLevel(1);
    BinaryenSetShrinkLevel(1);
    EM_ASM({
        CompiledTB = {};
    });

    /* The current code uses uint8_t for tcg operations. */
    tcg_debug_assert(tcg_op_defs_max <= UINT8_MAX);

    /* Registers available for 32 bit operations. */
    tcg_target_available_regs[TCG_TYPE_I32] = BIT(TCG_TARGET_NB_REGS) - 1;
    /* Registers available for 64 bit operations. */
    tcg_target_available_regs[TCG_TYPE_I64] = BIT(TCG_TARGET_NB_REGS) - 1;
    /* TODO: Which registers should be set here? */
    tcg_target_call_clobber_regs = BIT(TCG_TARGET_NB_REGS) - 1;

    s->reserved_regs = 0;
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_CALL_STACK);

    /* We use negative offsets from "sp" so that we can distinguish
       stores that might pretend to be call arguments.  */
    tcg_set_frame(s, TCG_REG_CALL_STACK,
                  -CPU_TEMP_BUF_NLONGS * sizeof(long),
                  CPU_TEMP_BUF_NLONGS * sizeof(long));

    create_invoker();
}

#define TODO() assert(111 * 0)

static BinaryenOp comparison32(TCGArg condition)
{
    switch (condition) {
    case TCG_COND_EQ:  return BinaryenEqInt32();
    case TCG_COND_NE:  return BinaryenNeInt32();
    case TCG_COND_LT:  return BinaryenLtSInt32();
    case TCG_COND_GE:  return BinaryenGeSInt32();
    case TCG_COND_LE:  return BinaryenLeSInt32();
    case TCG_COND_GT:  return BinaryenGtSInt32();
    case TCG_COND_LTU: return BinaryenLtUInt32();
    case TCG_COND_GEU: return BinaryenGeUInt32();
    case TCG_COND_LEU: return BinaryenLeUInt32();
    case TCG_COND_GTU: return BinaryenGtUInt32();
    default: TODO();
    }
}

static BinaryenOp comparison64(TCGArg condition)
{
    switch (condition) {
    case TCG_COND_EQ:  return BinaryenEqInt64();
    case TCG_COND_NE:  return BinaryenNeInt64();
    case TCG_COND_LT:  return BinaryenLtSInt64();
    case TCG_COND_GE:  return BinaryenGeSInt64();
    case TCG_COND_LE:  return BinaryenLeSInt64();
    case TCG_COND_GT:  return BinaryenGtSInt64();
    case TCG_COND_LTU: return BinaryenLtUInt64();
    case TCG_COND_GEU: return BinaryenGeUInt64();
    case TCG_COND_LEU: return BinaryenLeUInt64();
    case TCG_COND_GTU: return BinaryenGtUInt64();
    default: TODO();
    }
}

static void binaryen_out_reloc(TCGContext *s, TCGArg arg, BinaryenExpressionRef cond_expr)
{
    TCGLabel *label = arg_label(arg);
    tcg_out_expr(s, BinaryenNop(MODULE), 0); // so at least one insn in a block TODO: do we need it?
    tcg_out_expr(s, cond_expr, CONDITION_EXPR_ADDR);
    tcg_out_reloc(s, s->code_ptr, sizeof(tcg_target_ulong), label, 0);
    s->code_ptr++;
}

static const char *binaryen_ld_function(int *sign_ext_bits, int oi)
{
    switch (get_memop(oi) & (MO_BSWAP | MO_SSIZE)) {
    case MO_UB:   *sign_ext_bits = 0;  return "helper_ret_ldub_mmu";
    case MO_SB:   *sign_ext_bits = 24; return "helper_ret_ldub_mmu";
    case MO_LEUW: *sign_ext_bits = 0;  return "helper_le_lduw_mmu";
    case MO_LESW: *sign_ext_bits = 16; return "helper_le_lduw_mmu";
    case MO_LEUL: *sign_ext_bits = 0;  return "helper_le_ldul_mmu";
    case MO_BEUW: *sign_ext_bits = 0;  return "helper_be_lduw_mmu";
    case MO_BESW: *sign_ext_bits = 16; return "helper_be_lduw_mmu";
    case MO_BEUL: *sign_ext_bits = 0;  return "helper_be_ldul_mmu";
    case MO_LEQ:  *sign_ext_bits = 0;  return "helper_le_ldq_mmu";
    case MO_BEQ:  *sign_ext_bits = 0;  return "helper_be_ldq_mmu";
    default:
        tcg_abort();
    }
}

static const char *binaryen_st_function(int oi)
{
    switch (get_memop(oi) & (MO_BSWAP | MO_SIZE)) {
    case MO_UB:   return "helper_ret_stb_mmu";
    case MO_LEUW: return "helper_le_stw_mmu";
    case MO_LEUL: return "helper_le_stl_mmu";
    case MO_LEQ:  return "helper_le_stq_mmu";
    case MO_BEUW: return "helper_be_stw_mmu";
    case MO_BEUL: return "helper_be_stl_mmu";
    case MO_BEQ:  return "helper_be_stq_mmu";
    default:
        tcg_abort();
    }
}

/* Based on arm backend */
// TODO 64-bit guest
static BinaryenExpressionRef tcg_out_tlb_op(TCGContext *s, uint32_t addr_const, uint32_t addr_val, BinaryenExpressionRef slowpath, int oi, bool is_load, BinaryenExpressionRef value)
{
    int mem_index = get_mmuidx(oi);
    // Big endian ?
    TCGMemOp opc = get_memop(oi);
    bool is_signed = (opc & MO_SIGN) != 0;

    int cmp_off =
        (is_load
         ? offsetof(CPUArchState, tlb_table[mem_index][0].addr_read)
         : offsetof(CPUArchState, tlb_table[mem_index][0].addr_write));
    int add_off = offsetof(CPUArchState, tlb_table[mem_index][0].addend);

    // TMP0 <- virt_page
    tcg_out_expr(s, BinaryenSetLocal(MODULE, TLB_TMP0, BinaryenBinary(MODULE, BinaryenShrUInt32(),
        RI32(addr_const, addr_val),
        CONST32(TARGET_PAGE_BITS)
    )), 0);

    // TMP1 <- offset in TLB + TLB base address
    tcg_out_expr(s, BinaryenSetLocal(MODULE, TLB_TMP1,
        BinaryenBinary(MODULE, BinaryenAddInt32(),
            REG32(0),
            BinaryenBinary(MODULE, BinaryenShlInt32(),
                BinaryenBinary(MODULE, BinaryenAndInt32(),
                    BinaryenGetLocal(MODULE, TLB_TMP0, BinaryenTypeInt32()),
                    CONST32(CPU_TLB_SIZE - 1)
                ),
                CONST32(CPU_TLB_ENTRY_BITS)
            )
        )
    ), 0);

    tcg_out_expr(s, BinaryenSetLocal(MODULE, TLB_TMP2,
        BinaryenLoad(
            MODULE, 4, 0, cmp_off, 0, BinaryenTypeInt32(),
            BinaryenGetLocal(MODULE, TLB_TMP1, BinaryenTypeInt32())
        )
    ), 0);

    BinaryenExpressionRef comparator_virt_page = BinaryenBinary(MODULE, BinaryenShrUInt32(),
        BinaryenGetLocal(MODULE, TLB_TMP2, BinaryenTypeInt32()),
        CONST32(TARGET_PAGE_BITS)
    );

    unsigned s_bits = opc & MO_SIZE;
    unsigned a_bits = get_alignment_bits(opc);

    if (a_bits < s_bits) {
        a_bits = s_bits;
    }

    BinaryenExpressionRef use_slowpath = BinaryenBinary(MODULE, BinaryenOrInt32(),
        BinaryenBinary(MODULE, BinaryenNeInt32(),
            BinaryenGetLocal(MODULE, TLB_TMP0, BinaryenTypeInt32()),
            comparator_virt_page
        ),
        BinaryenBinary(MODULE, BinaryenAndInt32,
            BinaryenGetLocal(MODULE, TLB_TMP2, BinaryenTypeInt32()),
            CONST32((1 << TARGET_PAGE_BITS) - 1)
        )
    );
    if (a_bits > 0) {
        use_slowpath = BinaryenBinary(MODULE, BinaryenOrInt32(),
            use_slowpath,
            BinaryenBinary(MODULE, BinaryenAndInt32(), RI32(addr_const, addr_val), CONST32((1 << a_bits) - 1))
        );
    }

    BinaryenExpressionRef phys_addr = BinaryenBinary(MODULE, BinaryenAddInt32(),
        RI32(addr_const, addr_val),
        BinaryenLoad(
            MODULE, 4, 0, add_off, 0, BinaryenTypeInt32(),
            BinaryenGetLocal(MODULE, TLB_TMP1, BinaryenTypeInt32())
        )
    );

    BinaryenExpressionRef fastpath;
    if (is_load) {
        fastpath = BinaryenLoad(
            MODULE, 1 << s_bits, is_signed, 0, 0,
            (s_bits <= 2) ? BinaryenTypeInt32() : BinaryenTypeInt64() /* ??? */,
            phys_addr
        );
    } else {
        fastpath = BinaryenStore(
            MODULE, 1 << s_bits, 0, 0,
            phys_addr, value,
            (s_bits <= 2) ? BinaryenTypeInt32() : BinaryenTypeInt64()
        );
    }

    return BinaryenIf(MODULE, use_slowpath, slowpath, fastpath);
}

static inline void tcg_out_op(TCGContext *s, TCGOpcode opc,
                              const TCGArg *args, const int *const_args)
{
    int sign_ext_bits;
    BinaryenExpressionRef expr_tmp, extended_if_needed, ldst_args[6];
    BinaryenExpressionRef args_get[2];
    switch (opc) {
    case INDEX_op_exit_tb:
        tcg_out_expr(s, BinaryenReturn(MODULE, REG32(0)), 0);
        tcg_out_expr(s, 0, 0);
        break;
    case INDEX_op_goto_tb:
        if (s->tb_jmp_insn_offset) {
            /* Direct jump method. */
            TODO();
        } else {
            /* Indirect jump method. */

            // May overflow stack...
            args_get[0] = BinaryenGetLocal(MODULE, 0, BinaryenTypeInt32());
            args_get[1] = BinaryenGetLocal(MODULE, 1, BinaryenTypeInt32());
            BinaryenIf(MODULE, BinaryenLoad(MODULE, 4, 0, 0, 0, BinaryenTypeInt32(), CONST32((uintptr_t)(s->tb_jmp_target_addr + args[0]))),
                BinaryenCallIndirect(MODULE, BinaryenBinary(MODULE, BinaryenShrUInt32(),
                                     BinaryenBinary(MODULE, BinaryenSubInt32(),
                                                    BinaryenLoad(MODULE, 4, 0, 0, 0, BinaryenTypeInt32(), CONST32((uintptr_t)(s->tb_jmp_target_addr + args[0]))),
                                                    CONST32((uintptr_t)s->code_gen_buffer)
                                                   ),
                                     CONST32(2)),
                                     args_get, 2, BINARYEN_TB_FUNC_TYPE
                                    ),
                NULL
            );
            set_jmp_reset_offset(s, args[0]);
        }

        break;
    case INDEX_op_br:
        binaryen_out_reloc(s, args[0], CONST32(1));
        break;

#define OP_LD32(case_id, bytes, signed_) \
    case case_id:\
        /* TODO positive offset fastpath */ \
        STORE32(args[0], BinaryenLoad(MODULE, (bytes), (signed_), 0, 0, BinaryenTypeInt32(), BinaryenBinary(MODULE, BinaryenAddInt32(), CONST32(args[2]), ARG32(1)))); \
        break;
#define OP_ST32(case_id, bytes) \
    case case_id:\
        /* TODO positive offset fastpath */ \
        tcg_out_expr(s, BinaryenStore(MODULE, (bytes), 0, 0, BinaryenBinary(MODULE, BinaryenAddInt32(), CONST32(args[2]), ARG32(1)), ARG32(0), BinaryenTypeInt32()), 0); \
        break;

    OP_LD32(INDEX_op_ld8u_i32, 1, 0)
    OP_LD32(INDEX_op_ld8s_i32, 1, 1)
    OP_LD32(INDEX_op_ld16u_i32, 2, 0)
    OP_LD32(INDEX_op_ld16s_i32, 2, 1)
    OP_LD32(INDEX_op_ld_i32, 4, 0)

    OP_ST32(INDEX_op_st8_i32, 1)
    OP_ST32(INDEX_op_st16_i32, 2)
    OP_ST32(INDEX_op_st_i32, 4)

//     OP_LD(INDEX_op_ld8u_i64, 1, 0, BinaryenTypeInt64())
//     OP_LD(INDEX_op_ld8s_i64, 1, 1, BinaryenTypeInt64())
//     OP_LD(INDEX_op_ld16u_i64, 2, 0, BinaryenTypeInt64())
//     OP_LD(INDEX_op_ld16s_i64, 2, 1, BinaryenTypeInt64())
//     OP_LD(INDEX_op_ld32u_i64, 4, 0, BinaryenTypeInt64())
//     OP_LD(INDEX_op_ld32s_i64, 4, 1, BinaryenTypeInt64())
//     OP_LD(INDEX_op_ld_i64, 8, 0, BinaryenTypeInt64())
//
//     OP_ST(INDEX_op_st8_i64, 1, RI64, BinaryenTypeInt64())
//     OP_ST(INDEX_op_st16_i64, 2, RI64, BinaryenTypeInt64())
//     OP_ST(INDEX_op_st32_i64, 4, RI64, BinaryenTypeInt64())
//     OP_ST(INDEX_op_st_i64, 8, RI64, BinaryenTypeInt64())

#undef OP_LD32
#undef OP_ST32

#define UN_OP32(case_id, op) \
    case case_id: \
        UNARY32(op, args[0], ARG32(1)); \
        break;
#define BIN_OP32(case_id, op) \
    case case_id: \
        BINARY32(op, args[0], ARG32(1), ARG32(2)); \
        break;
#define BIN_OP_4_8_8(case_id, op) \
    case case_id: \
        STORE32(args[0], BinaryenUnary(MODULE, BinaryenWrapInt64(), BinaryenBinary(MODULE, op, RI64_2reg(const_args[1], args[1], const_args[2], args[2]), RI64_2reg(const_args[3], args[3], const_args[4], args[4])))); \
        break;
#define BIN_OP_8_8_8(case_id, op) \
    case case_id: \
        BINARY64(op, args[0], args[1], RI64_2reg(const_args[2], args[2], const_args[3], args[3]), RI64_2reg(const_args[4], args[4], const_args[5], args[5])); \
        break;

    BIN_OP32(INDEX_op_setcond_i32, comparison32(args[3]))
    BIN_OP_4_8_8(INDEX_op_setcond2_i32, comparison64(args[5]))

    BIN_OP32(INDEX_op_add_i32, BinaryenAddInt32())
    BIN_OP32(INDEX_op_sub_i32, BinaryenSubInt32())
    BIN_OP32(INDEX_op_mul_i32, BinaryenMulInt32())
    BIN_OP32(INDEX_op_and_i32, BinaryenAndInt32())
//    BIN_OP(INDEX_op_andc_i32)     /* Optional (TCG_TARGET_HAS_andc_i32). */
//    BIN_OP(INDEX_op_eqv_i32)      /* Optional (TCG_TARGET_HAS_eqv_i32). */
//    BIN_OP(INDEX_op_nand_i32)     /* Optional (TCG_TARGET_HAS_nand_i32). */
//    BIN_OP(INDEX_op_nor_i32)      /* Optional (TCG_TARGET_HAS_nor_i32). */
    BIN_OP32(INDEX_op_or_i32, BinaryenOrInt32())
//    BIN_OP(INDEX_op_orc_i32)      /* Optional (TCG_TARGET_HAS_orc_i32). */
    BIN_OP32(INDEX_op_xor_i32, BinaryenXorInt32())
    BIN_OP32(INDEX_op_shl_i32, BinaryenShlInt32())
    BIN_OP32(INDEX_op_shr_i32, BinaryenShrUInt32())
    BIN_OP32(INDEX_op_sar_i32, BinaryenShrSInt32())
    BIN_OP32(INDEX_op_rotl_i32, BinaryenRotLInt32())     /* Optional (TCG_TARGET_HAS_rot_i32). */
    BIN_OP32(INDEX_op_rotr_i32, BinaryenRotRInt32())     /* Optional (TCG_TARGET_HAS_rot_i32). */
//    case INDEX_op_deposit_i32:  /* Optional (TCG_TARGET_HAS_deposit_i32). */
//        tcg_out_r(s, args[0]);
//        tcg_out_r(s, args[1]);
//        tcg_out_r(s, args[2]);
//        tcg_debug_assert(args[3] <= UINT8_MAX);
//        tcg_out8(s, args[3]);
//        tcg_debug_assert(args[4] <= UINT8_MAX);
//        tcg_out8(s, args[4]);
//        break;


//    UN_OP(INDEX_op_neg_i32)      /* Optional (TCG_TARGET_HAS_neg_i32). */
//    UN_OP(INDEX_op_not_i32)      /* Optional (TCG_TARGET_HAS_not_i32). */
    UN_OP32(INDEX_op_ext8s_i32, BinaryenExtendS8Int32())    /* Optional (TCG_TARGET_HAS_ext8s_i32). */
    UN_OP32(INDEX_op_ext16s_i32, BinaryenExtendS16Int32())   /* Optional (TCG_TARGET_HAS_ext16s_i32). */
    UN_OP32(INDEX_op_ctpop_i32, BinaryenPopcntInt32())
//    UN_OP(INDEX_op_ext8u_i32)    /* Optional (TCG_TARGET_HAS_ext8u_i32). */
//    UN_OP(INDEX_op_ext16u_i32)   /* Optional (TCG_TARGET_HAS_ext16u_i32). */
//    UN_OP(INDEX_op_bswap16_i32)  /* Optional (TCG_TARGET_HAS_bswap16_i32). */
//    UN_OP(INDEX_op_bswap32_i32)  /* Optional (TCG_TARGET_HAS_bswap32_i32). */


//    case INDEX_op_div_i32:      /* Optional (TCG_TARGET_HAS_div_i32). */
//    case INDEX_op_divu_i32:     /* Optional (TCG_TARGET_HAS_div_i32). */
//    case INDEX_op_rem_i32:      /* Optional (TCG_TARGET_HAS_div_i32). */
//    case INDEX_op_remu_i32:     /* Optional (TCG_TARGET_HAS_div_i32). */
//        tcg_out_r(s, args[0]);
//        tcg_out_ri32(s, const_args[1], args[1]);
//        tcg_out_ri32(s, const_args[2], args[2]);
//        break;
//    case INDEX_op_div2_i32:     /* Optional (TCG_TARGET_HAS_div2_i32). */
//    case INDEX_op_divu2_i32:    /* Optional (TCG_TARGET_HAS_div2_i32). */
//        TODO();
//
    BIN_OP_8_8_8(INDEX_op_add2_i32, BinaryenAddInt64())
    BIN_OP_8_8_8(INDEX_op_sub2_i32, BinaryenSubInt64())

    case INDEX_op_brcond2_i32:
        binaryen_out_reloc(s, args[5], BinaryenBinary(MODULE, comparison64(args[4]), RI64_2reg(const_args[0], args[0], const_args[1], args[1]), RI64_2reg(const_args[2], args[2], const_args[3], args[3])));
        break;
    case INDEX_op_mulu2_i32:
        BINARY64(BinaryenMulInt64(), args[0], args[1],
                 BinaryenUnary(MODULE, BinaryenExtendUInt32(), ARG32(2)),
                 BinaryenUnary(MODULE, BinaryenExtendUInt32(), ARG32(3))
        );
        break;
    case INDEX_op_brcond_i32:
        binaryen_out_reloc(s, args[3], BinaryenBinary(MODULE, comparison32(args[2]), ARG32(0), ARG32(1)));
        break;
    case INDEX_op_movcond_i32:
        STORE32(args[0], BinaryenIf(MODULE, BinaryenBinary(MODULE, comparison32(args[5]), ARG32(1), ARG32(2)), ARG32(3), ARG32(4)));
        break;
    case INDEX_op_clz_i32:
        STORE32(args[0], BinaryenIf(MODULE, ARG32(1), BinaryenUnary(MODULE, BinaryenClzInt32(), ARG32(1)), ARG32(2)));
        break;
    case INDEX_op_ctz_i32:
        STORE32(args[0], BinaryenIf(MODULE, ARG32(1), BinaryenUnary(MODULE, BinaryenCtzInt32(), ARG32(1)), ARG32(2)));
        break;

    case INDEX_op_qemu_ld_i32:
        // args[0] -- dest, args[1] -- taddr, args[2] -- oi
        ldst_args[0] = REG32(0);
        ldst_args[1] = ARG32(1);
        ldst_args[2] = CONST32(args[2]);
        ldst_args[3] = CONST32((uintptr_t)s->code_ptr);
        expr_tmp = BinaryenCall(MODULE, binaryen_ld_function(&sign_ext_bits, args[2]), ldst_args, 4, BinaryenTypeInt32());
        if (sign_ext_bits) {
            extended_if_needed = BinaryenUnary(MODULE, sign_ext_bits == 16 ? BinaryenExtendS16Int32() : BinaryenExtendS8Int32(), expr_tmp); // TODO args[0] -- imm
        } else {
            extended_if_needed = BinaryenBinary(MODULE, BinaryenAndInt32(), expr_tmp, CONST32((0xFFFFFFFFu << sign_ext_bits) >> sign_ext_bits));
        }
        STORE32(args[0], tcg_out_tlb_op(s, const_args[1], args[1], extended_if_needed, args[2], 1, NULL));
        break;
    case INDEX_op_qemu_ld_i64:
        // (hi = args[1], lo = args[0]) -- dest, args[2] -- taddr, args[3] -- oi
        assert((get_memop(args[3]) & MO_SIZE) == MO_Q);
        ldst_args[0] = REG32(0);
        ldst_args[1] = ARG32(2);
        ldst_args[2] = CONST32(args[3]);
        ldst_args[3] = CONST32((uintptr_t)s->code_ptr);
        STORE32(args[0], BinaryenCall(MODULE, binaryen_ld_function(&sign_ext_bits, args[3]), ldst_args, 4, BinaryenTypeInt32()));
        STORE32(args[1], BinaryenCall(MODULE, "get_temp_ret", NULL, 0, BinaryenTypeInt32()));
        break;
    case INDEX_op_qemu_st_i32:
        // args[0] -- value, args[1] -- taddr, args[2] -- oi
        ldst_args[0] = REG32(0);
        ldst_args[1] = ARG32(1);
        ldst_args[2] = ARG32(0);
        ldst_args[3] = CONST32(args[2]);
        ldst_args[4] = CONST32((uintptr_t)s->code_ptr);
        tcg_out_expr(s, tcg_out_tlb_op(
            s, const_args[1], args[1],
            BinaryenCall(MODULE, binaryen_st_function(args[2]), ldst_args, 5, BinaryenTypeNone()),
            args[2], 0, ARG32(0)
        ), 0);
        break;
    case INDEX_op_qemu_st_i64:
        // (hi = args[1], lo = args[0]) -- value, args[2] -- taddr, args[3] -- oi
        assert((get_memop(args[3]) & MO_SIZE) == MO_Q);
        ldst_args[0] = REG32(0);
        ldst_args[1] = ARG32(2);

        ldst_args[2] = ARG32(0);
        ldst_args[3] = ARG32(1);

        ldst_args[4] = CONST32(args[3]);
        ldst_args[5] = CONST32((uintptr_t)s->code_ptr);
        tcg_out_expr(s, BinaryenCall(MODULE, binaryen_st_function(args[3]), ldst_args, 6, BinaryenTypeNone()), 0);
        break;
    case INDEX_op_mb:
        break;
    case INDEX_op_mov_i32:  /* Always emitted via tcg_out_mov.  */
    case INDEX_op_mov_i64:
    case INDEX_op_movi_i32: /* Always emitted via tcg_out_movi.  */
    case INDEX_op_movi_i64:
    case INDEX_op_call:     /* Always emitted via tcg_out_call.  */
    default:
        assert(234 * 0);
        tcg_abort();
    }
}

static void tcg_target_qemu_prologue(TCGContext *s)
{
}

static void tcg_out_mov(TCGContext *s, TCGType type, TCGReg ret, TCGReg arg)
{
    STORE32(ret, REG32(arg));
}

static void tcg_out_movi(TCGContext *s, TCGType type,
                         TCGReg ret, tcg_target_long arg)
{
    assert (arg == (uint32_t)arg);
    STORE32(ret, CONST32(arg));
}

static void tcg_out_ld(TCGContext *s, TCGType type, TCGReg ret,
                       TCGReg arg1, intptr_t arg2)
{
    assert(type == TCG_TYPE_I32);
    static int const_args[] = {0, 0, 1};
    TCGArg args[] = {ret, arg1, arg2};
    tcg_out_op(s, INDEX_op_ld_i32, args, const_args);
}


static void tcg_out_st(TCGContext *s, TCGType type, TCGReg arg,
                       TCGReg arg1, intptr_t arg2)
{
    assert(type == TCG_TYPE_I32);
    static int const_args[] = {0, 0, 1};
    TCGArg args[] = {arg, arg1, arg2};
    tcg_out_op(s, INDEX_op_st_i32, args, const_args);
}

static bool tcg_out_sti(TCGContext *s, TCGType type, TCGArg val,
                        TCGReg base, intptr_t ofs)
{
    return false;
}

static inline void tcg_out_call(TCGContext *s, tcg_insn_unit *dest)
{
    BinaryenExpressionRef call_operands[ARRAY_SIZE(tcg_target_call_iarg_regs) + 1];

    call_operands[0] = CONST32((uint32_t)dest);
    for (int i = 1; i < ARRAY_SIZE(call_operands); ++i) {
        call_operands[i] = REG32(tcg_target_call_iarg_regs[i - 1]);
    }

    STORE32(tcg_target_call_oarg_regs[0], BinaryenCall(
        MODULE,
        "call_helper",
        call_operands,
        ARRAY_SIZE(call_operands),
        BinaryenTypeInt32()
    ));
    STORE32(tcg_target_call_oarg_regs[1], BinaryenCall(MODULE, "get_temp_ret", NULL, 0, BinaryenTypeInt32()));
}

static void patch_reloc(tcg_insn_unit *code_ptr, int type,
                        intptr_t value, intptr_t addend)
{
    assert((((uintptr_t)value) & 0x3) == 0);
    *(uint32_t*)code_ptr = COND_JUMP_ADDR | (uint32_t)value;
}

#define R       "r"
#define RI      "ri"
#if TCG_TARGET_REG_BITS == 32
# define R64    "r", "r"
#else
# define R64    "r"
#endif
#if TARGET_LONG_BITS > TCG_TARGET_REG_BITS
# define L      "L", "L"
# define S      "S", "S"
#else
# define L      "L"
# define S      "S"
#endif

static const TCGTargetOpDef tcg_target_op_defs[] = {
    { INDEX_op_exit_tb, { NULL } },
    { INDEX_op_goto_tb, { NULL } },
    { INDEX_op_br, { NULL } },

    { INDEX_op_ld8u_i32, { R, RI } },
    { INDEX_op_ld8s_i32, { R, RI } },
    { INDEX_op_ld16u_i32, { R, RI } },
    { INDEX_op_ld16s_i32, { R, RI } },
    { INDEX_op_ld_i32, { R, RI } },
    { INDEX_op_st8_i32, { R, RI } },
    { INDEX_op_st16_i32, { R, RI } },
    { INDEX_op_st_i32, { R, RI } },

    { INDEX_op_add_i32, { R, RI, RI } },
    { INDEX_op_sub_i32, { R, RI, RI } },
    { INDEX_op_mul_i32, { R, RI, RI } },
#if TCG_TARGET_HAS_div_i32
    { INDEX_op_div_i32, { R, RI, RI } },
    { INDEX_op_divu_i32, { R, RI, RI } },
    { INDEX_op_rem_i32, { R, RI, RI } },
    { INDEX_op_remu_i32, { R, RI, RI } },
#elif TCG_TARGET_HAS_div2_i32
    { INDEX_op_div2_i32, { R, R, "0", "1", R } },
    { INDEX_op_divu2_i32, { R, R, "0", "1", R } },
#endif
    /* TODO: Does R, RI, RI result in faster code than R, R, RI?
       If both operands are constants, we can optimize. */
    { INDEX_op_and_i32, { R, RI, RI } },
#if TCG_TARGET_HAS_andc_i32
    { INDEX_op_andc_i32, { R, RI, RI } },
#endif
#if TCG_TARGET_HAS_eqv_i32
    { INDEX_op_eqv_i32, { R, RI, RI } },
#endif
#if TCG_TARGET_HAS_nand_i32
    { INDEX_op_nand_i32, { R, RI, RI } },
#endif
#if TCG_TARGET_HAS_nor_i32
    { INDEX_op_nor_i32, { R, RI, RI } },
#endif
    { INDEX_op_or_i32, { R, RI, RI } },
#if TCG_TARGET_HAS_orc_i32
    { INDEX_op_orc_i32, { R, RI, RI } },
#endif
    { INDEX_op_xor_i32, { R, RI, RI } },
    { INDEX_op_shl_i32, { R, RI, RI } },
    { INDEX_op_shr_i32, { R, RI, RI } },
    { INDEX_op_sar_i32, { R, RI, RI } },
#if TCG_TARGET_HAS_rot_i32
    { INDEX_op_rotl_i32, { R, RI, RI } },
    { INDEX_op_rotr_i32, { R, RI, RI } },
#endif
#if TCG_TARGET_HAS_deposit_i32
    { INDEX_op_deposit_i32, { R, "0", R } },
#endif

    { INDEX_op_brcond_i32, { R, RI } },

    { INDEX_op_setcond_i32, { R, R, RI } },
#if TCG_TARGET_REG_BITS == 64
    { INDEX_op_setcond_i64, { R, R, RI } },
#endif /* TCG_TARGET_REG_BITS == 64 */

#if TCG_TARGET_REG_BITS == 32
    /* TODO: Support R, R, R, R, RI, RI? Will it be faster? */
    { INDEX_op_add2_i32, { R, R, RI, RI, RI, RI } },
    { INDEX_op_sub2_i32, { R, R, RI, RI, RI, RI } },
    { INDEX_op_brcond2_i32, { R, R, RI, RI } },
    { INDEX_op_mulu2_i32, { R, R, RI, RI } },
    { INDEX_op_setcond2_i32, { R, R, R, RI, RI } },
#endif

#if TCG_TARGET_HAS_not_i32
    { INDEX_op_not_i32, { R, R } },
#endif
#if TCG_TARGET_HAS_neg_i32
    { INDEX_op_neg_i32, { R, R } },
#endif

#if TCG_TARGET_HAS_movcond_i32
    { INDEX_op_movcond_i32, { R, RI, RI, RI, RI, RI } },
#endif
#if TCG_TARGET_HAS_ctpop_i32
    { INDEX_op_ctpop_i32, { R, R } },
#endif
#if TCG_TARGET_HAS_clz_i32
    { INDEX_op_clz_i32, { R, RI, RI } },
#endif
#if TCG_TARGET_HAS_ctz_i32
    { INDEX_op_ctz_i32, { R, RI, RI } },
#endif

    { INDEX_op_qemu_ld_i32, { R, L } },
    { INDEX_op_qemu_ld_i64, { R64, L } },

    { INDEX_op_qemu_st_i32, { R, S } },
    { INDEX_op_qemu_st_i64, { R64, S } },

#if TCG_TARGET_HAS_ext8s_i32
    { INDEX_op_ext8s_i32, { R, R } },
#endif
#if TCG_TARGET_HAS_ext16s_i32
    { INDEX_op_ext16s_i32, { R, R } },
#endif
#if TCG_TARGET_HAS_ext8u_i32
    { INDEX_op_ext8u_i32, { R, R } },
#endif
#if TCG_TARGET_HAS_ext16u_i32
    { INDEX_op_ext16u_i32, { R, R } },
#endif

#if TCG_TARGET_HAS_bswap16_i32
    { INDEX_op_bswap16_i32, { R, R } },
#endif
#if TCG_TARGET_HAS_bswap32_i32
    { INDEX_op_bswap32_i32, { R, R } },
#endif

    { INDEX_op_mb, { } },
    { -1 },
};

static const TCGTargetOpDef *tcg_target_op_def(TCGOpcode op)
{
    int i, n = ARRAY_SIZE(tcg_target_op_defs);

    for (i = 0; i < n; ++i) {
        if (tcg_target_op_defs[i].op == op) {
            return &tcg_target_op_defs[i];
        }
    }
    return NULL;
}

// No target-specific requirements TODO: signedness / size?

static const char *target_parse_constraint(TCGArgConstraint *ct,
                                           const char *ct_str, TCGType type)
{
    switch (*ct_str++) {
    case 'r':
    case 'L':                   /* qemu_ld constraint */
    case 'S':                   /* qemu_st constraint */
        ct->ct |= TCG_CT_REG;
        ct->u.regs = BIT(TCG_TARGET_NB_REGS) - 1;
        break;
    default:
        return NULL;
    }
    return ct_str;
}

#define FLAG_FROM_PTR(x) ((x) & 0x3)
#define PTR_FROM_PTR(x)  ((void *)(((uint32_t)(x)) & ~0x3))
void flush_icache_range(uintptr_t _start, uintptr_t _stop)
{
    uint32_t *start = (void *)_start;
    uint32_t *stop  = (void *)_stop;
    TCGContext *s = (TCGContext *)start[1];
    RelooperRef relooper = RelooperCreate(MODULE);

    assert(start[0] == 0);

    uint32_t *begin = (uint32_t *)(start + 2);
    uint32_t *end   = (uint32_t *)stop;

    if (begin >= end) {
        fprintf(stderr, "Empty TB\n");
        start[0] = 0;
        return;
    }

    // create basic blocks (not efficiently)
    uint32_t *bb_start = begin;
    int skip = 0;
    while (bb_start < end) {
        assert(FLAG_FROM_PTR(*bb_start) == 0);
        uint32_t *bb_end = end;

        // detect implicit boundaries (jump targets)
        for (uint32_t *scan_ptr = begin; scan_ptr < end; ++scan_ptr) {
            uint32_t *orig_target = PTR_FROM_PTR(*scan_ptr);
            if (FLAG_FROM_PTR(*scan_ptr) == COND_JUMP_ADDR) {
                if (orig_target > bb_start && orig_target < bb_end) {
                    skip = 0;
                    bb_end = orig_target;
                }
            }
        }

        // detect explicit boundaries (jumps to other BBs)
        for (uint32_t *scan_ptr = bb_start; scan_ptr < bb_end; ++scan_ptr) {
            if (FLAG_FROM_PTR(*scan_ptr) == CONDITION_EXPR_ADDR) {
                skip = 2;
                bb_end = scan_ptr; // not dereferenced!
                break;
            }
            assert(FLAG_FROM_PTR(*scan_ptr) == 0);
            if (BinaryenExpressionGetId(*scan_ptr) == BinaryenReturnId()) {
                skip = 0;
                bb_end = scan_ptr + 1;
                break;
            }
        }

        assert(bb_start != bb_end);
        BinaryenExpressionRef block = BinaryenBlock(MODULE, NULL, bb_start, bb_end - bb_start, BinaryenTypeNone());
        RelooperBlockRef relooper_block = RelooperAddBlock(relooper, block);
        *bb_start = BLOCK_ADDR | (uint32_t)relooper_block;
//         fprintf(stderr, "*%p := %p\n", bb_start, *bb_start);

        bb_start = bb_end + skip;
        if (*bb_start == 0) {
            // just after Return
            bb_start++;
        }
    }

    // set up branches between blocks
    RelooperBlockRef from = (RelooperBlockRef)PTR_FROM_PTR(*begin);
    RelooperBlockRef to;
    for (uint32_t *scan_ptr = begin + 1; scan_ptr < end; ++scan_ptr) {
        if (*scan_ptr == 0) {
            // just after return
            from = NULL;
        }
        if (FLAG_FROM_PTR(*scan_ptr) == BLOCK_ADDR) {
            if (FLAG_FROM_PTR(scan_ptr[-1]) == COND_JUMP_ADDR) {
                assert(FLAG_FROM_PTR(scan_ptr[-2]) == CONDITION_EXPR_ADDR);
                to = (RelooperBlockRef)PTR_FROM_PTR(*(uint32_t *)PTR_FROM_PTR(scan_ptr[-1]));
                RelooperAddBranch(from, to, (BinaryenExpressionRef)PTR_FROM_PTR(scan_ptr[-2]), NULL);
            }
            to = (RelooperBlockRef)PTR_FROM_PTR(scan_ptr[0]);
            if (from != NULL) {
                RelooperAddBranch(from, to, NULL, NULL);
            }
            from = (RelooperBlockRef)PTR_FROM_PTR(scan_ptr[0]);
        }
    }

    start[0] = (uint32_t)RelooperRenderAndDispose(relooper, PTR_FROM_PTR(*begin), TCG_TARGET_NB_REGS);
    compile_module(s, MODULE, start, start[0]);
}

static inline int tcg_target_const_match(tcg_target_long val, TCGType type,
                                         const TCGArgConstraint *arg_ct)
{
    int ct = arg_ct->ct;
    if (ct & TCG_CT_CONST) {
        return 1;
    }
    return 0;
}
