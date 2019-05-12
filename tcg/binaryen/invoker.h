/*
 * Tiny Code Generator for QEMU - Binaryen backend
 *
 * Copyright (c) 2018-2019 Anatoly Trosinenko
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

#ifndef __TCG_BINARYEN_INVOKER_H
#define __TCG_BINARYEN_INVOKER_H

#ifdef __cplusplus
extern "C" {
#endif

#undef stringify
#include <binaryen-c.h>

#define TLB_TMP0 (TCG_TARGET_NB_REGS)
#define TLB_TMP1 (TCG_TARGET_NB_REGS + 1)
#define TLB_TMP2 (TCG_TARGET_NB_REGS + 2)
#define TMP64    (TCG_TARGET_NB_REGS + 3)


#define INTERPRET_MARKER (-2)
#define ENTRY_MARKER (-1)

typedef uint64_t (*helper_func)(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5, uint32_t arg6, uint32_t arg7, uint32_t arg8, uint32_t arg9, uint32_t arg10, uint32_t arg11, uint32_t arg12);
uint64_t call_helper(helper_func func, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5, uint32_t arg6, uint32_t arg7, uint32_t arg8, uint32_t arg9, uint32_t arg10, uint32_t arg11, uint32_t arg12);

// 2 x 32bit arg-regs + (NB_REGS - 2) x 32 bit + 3 x (32 bit TLB) + 1 x (64 bit TMP64)
#define FUNC_LOCALS_COUNT (TCG_TARGET_NB_REGS + 2)

extern uintptr_t (*invoke_tb)(int, void *, uintptr_t);
extern BinaryenFunctionTypeRef helper_type, ld_type, st32_type, st64_type, tb_func_type, get_temp_ret_type;
extern BinaryenType func_locals[];
extern long tcg_temps[], *tcg_temps_end;

struct TranslationBlock;

void delete_instance(struct TranslationBlock *ptr, void *_wi);
void *prepare_module(BinaryenModuleRef module, BinaryenExpressionRef expr);
void compile_module(TranslationBlock *tb, void *_wi);
uintptr_t interpret_module(void *_module, void *env, uintptr_t sp_value);

int get_fptr(struct TranslationBlock *tb);
void create_invoker(void);

#ifdef __cplusplus
}
#endif

#endif
