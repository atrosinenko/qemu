/*
 * Tiny Code Generator for QEMU - Binaryen backend
 *
 * Copyright (c) 2018 Anatoly Trosinenko
 * Based on tcg/{i386,tci}/tcg-target.h
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

#ifndef BINARYEN_TCG_TARGET_H
#define BINARYEN_TCG_TARGET_H

// insn = BinaryenExpressionRef, sizeof(void *) == 4
#define TCG_TARGET_INSN_UNIT_SIZE  4

#define CONFIG_BINARYEN

#define HAVE_TCG_QEMU_TB_EXEC
#define TCG_TARGET_TLB_DISPLACEMENT_BITS 16

#define TCG_TARGET_REG_BITS  32
#define TCG_TARGET_NB_REGS   31


#define TCG_REG_CALL_STACK 1
#define TCG_TARGET_STACK_ALIGN 16
#define TCG_TARGET_CALL_STACK_OFFSET 0

typedef int TCGReg;

/* optional instructions */
#define TCG_TARGET_HAS_div2_i32         0
#define TCG_TARGET_HAS_rot_i32          1
#define TCG_TARGET_HAS_ext8s_i32        1
#define TCG_TARGET_HAS_ext16s_i32       1
#define TCG_TARGET_HAS_ext8u_i32        0
#define TCG_TARGET_HAS_ext16u_i32       0
#define TCG_TARGET_HAS_bswap16_i32      0
#define TCG_TARGET_HAS_bswap32_i32      0
#define TCG_TARGET_HAS_neg_i32          0
#define TCG_TARGET_HAS_not_i32          0
#define TCG_TARGET_HAS_andc_i32         0
#define TCG_TARGET_HAS_orc_i32          0
#define TCG_TARGET_HAS_eqv_i32          0
#define TCG_TARGET_HAS_nand_i32         0
#define TCG_TARGET_HAS_nor_i32          0
#define TCG_TARGET_HAS_clz_i32          1
#define TCG_TARGET_HAS_ctz_i32          1
#define TCG_TARGET_HAS_ctpop_i32        1
#define TCG_TARGET_HAS_deposit_i32      0
#define TCG_TARGET_HAS_extract_i32      0
#define TCG_TARGET_HAS_sextract_i32     0
#define TCG_TARGET_HAS_movcond_i32      1
#define TCG_TARGET_HAS_add2_i32         1
#define TCG_TARGET_HAS_sub2_i32         1
#define TCG_TARGET_HAS_mulu2_i32        1
#define TCG_TARGET_HAS_muls2_i32        0
#define TCG_TARGET_HAS_muluh_i32        0
#define TCG_TARGET_HAS_mulsh_i32        0
#define TCG_TARGET_HAS_goto_ptr         0
#define TCG_TARGET_HAS_direct_jump      0


#define TCG_AREG0 0

void flush_icache_range(uintptr_t start, uintptr_t stop);


#define BLOCK_ADDR          0x1
#define CONDITION_EXPR_ADDR 0x2
#define COND_JUMP_ADDR      0x3


static inline void tb_target_set_jmp_target(uintptr_t tc_ptr,
                                            uintptr_t jmp_addr, uintptr_t addr)
{
    if (jmp_addr != addr - 4) {
        *(int32_t *)jmp_addr = addr | COND_JUMP_ADDR;
//      assert(*(uint32_t *)tc_ptr == 0);
    } else {
        *(int32_t *)jmp_addr = COND_JUMP_ADDR;
    }
}

// TODO
#define TCG_TARGET_DEFAULT_MO 0

#endif // BINARYEN_TCG_TARGET_H
