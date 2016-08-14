/* Helper file for declaring TCG helper functions.
   This one expands generation functions for tcg opcodes.  */

#ifndef HELPER_GEN_H
#define HELPER_GEN_H 1
#if 1
#define HELPER_WR(name) glue(name,_wrapper)
#else
#define HELPER_WR(name) HELPER(name)
#endif
#include <exec/helper-head.h>

#if TCG_TARGET_REG_BITS == 32
#define ARGS int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10
#endif

#define DEF_HELPER_FLAGS_0(name, flags, ret)                            \
long long HELPER_WR(name)(ARGS);                                        \
static inline void glue(gen_helper_, name)(dh_retvar_decl0(ret))        \
{                                                                       \
  tcg_gen_callN(&tcg_ctx, HELPER_WR(name), dh_retvar(ret), 0, NULL);       \
}

#define DEF_HELPER_FLAGS_1(name, flags, ret, t1)                        \
long long HELPER_WR(name)(ARGS);                                        \
static inline void glue(gen_helper_, name)(dh_retvar_decl(ret)          \
    dh_arg_decl(t1, 1))                                                 \
{                                                                       \
  TCGArg args[1] = { dh_arg(t1, 1) };                                   \
  tcg_gen_callN(&tcg_ctx, HELPER_WR(name), dh_retvar(ret), 1, args);       \
}

#define DEF_HELPER_FLAGS_2(name, flags, ret, t1, t2)                    \
long long HELPER_WR(name)(ARGS);                                        \
static inline void glue(gen_helper_, name)(dh_retvar_decl(ret)          \
    dh_arg_decl(t1, 1), dh_arg_decl(t2, 2))                             \
{                                                                       \
  TCGArg args[2] = { dh_arg(t1, 1), dh_arg(t2, 2) };                    \
  tcg_gen_callN(&tcg_ctx, HELPER_WR(name), dh_retvar(ret), 2, args);       \
}

#define DEF_HELPER_FLAGS_3(name, flags, ret, t1, t2, t3)                \
long long HELPER_WR(name)(ARGS);                                        \
static inline void glue(gen_helper_, name)(dh_retvar_decl(ret)          \
    dh_arg_decl(t1, 1), dh_arg_decl(t2, 2), dh_arg_decl(t3, 3))         \
{                                                                       \
  TCGArg args[3] = { dh_arg(t1, 1), dh_arg(t2, 2), dh_arg(t3, 3) };     \
  tcg_gen_callN(&tcg_ctx, HELPER_WR(name), dh_retvar(ret), 3, args);       \
}

#define DEF_HELPER_FLAGS_4(name, flags, ret, t1, t2, t3, t4)            \
long long HELPER_WR(name)(ARGS);                                        \
static inline void glue(gen_helper_, name)(dh_retvar_decl(ret)          \
    dh_arg_decl(t1, 1), dh_arg_decl(t2, 2),                             \
    dh_arg_decl(t3, 3), dh_arg_decl(t4, 4))                             \
{                                                                       \
  TCGArg args[4] = { dh_arg(t1, 1), dh_arg(t2, 2),                      \
                     dh_arg(t3, 3), dh_arg(t4, 4) };                    \
  tcg_gen_callN(&tcg_ctx, HELPER_WR(name), dh_retvar(ret), 4, args);       \
}

#define DEF_HELPER_FLAGS_5(name, flags, ret, t1, t2, t3, t4, t5)        \
long long HELPER_WR(name)(ARGS);                                        \
static inline void glue(gen_helper_, name)(dh_retvar_decl(ret)          \
    dh_arg_decl(t1, 1),  dh_arg_decl(t2, 2), dh_arg_decl(t3, 3),        \
    dh_arg_decl(t4, 4), dh_arg_decl(t5, 5))                             \
{                                                                       \
  TCGArg args[5] = { dh_arg(t1, 1), dh_arg(t2, 2), dh_arg(t3, 3),       \
                     dh_arg(t4, 4), dh_arg(t5, 5) };                    \
  tcg_gen_callN(&tcg_ctx, HELPER_WR(name), dh_retvar(ret), 5, args);       \
}

#include "helper.h"
#include "trace/generated-helpers.h"
#include "trace/generated-helpers-wrappers.h"
#include "tcg-runtime.h"

#undef DEF_HELPER_FLAGS_0
#undef DEF_HELPER_FLAGS_1
#undef DEF_HELPER_FLAGS_2
#undef DEF_HELPER_FLAGS_3
#undef DEF_HELPER_FLAGS_4
#undef DEF_HELPER_FLAGS_5
#undef GEN_HELPER

#endif /* HELPER_GEN_H */
