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

extern "C" {
#include "qemu/osdep.h"
#include "tcg-target.h"
#include "qemu/bswap.h"
}

#include "invoker.h"


#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EM_ASM(x, ...)
#define EM_ASM_INT(x, ...) 0
#endif

#include "support/name.h"
#include "wasm.h"
#include "wasm-interpreter.h"
#include "ir/module-utils.h"


using namespace wasm;

extern "C" {

/* Value zero-extended to tcg register size.  */
uint32_t helper_ret_ldub_mmu(uint32_t env, uint32_t addr, uint32_t oi, uint32_t retaddr);
uint32_t helper_le_lduw_mmu(uint32_t env, uint32_t addr, uint32_t oi, uintptr_t retaddr);
uint32_t helper_le_ldul_mmu(uint32_t env, uint32_t addr, uint32_t oi, uintptr_t retaddr);
uint64_t helper_le_ldq_mmu(uint32_t env, uint32_t addr, uint32_t oi, uintptr_t retaddr);
uint32_t helper_be_lduw_mmu(uint32_t env, uint32_t addr, uint32_t oi, uintptr_t retaddr);
uint32_t helper_be_ldul_mmu(uint32_t env, uint32_t addr, uint32_t oi, uintptr_t retaddr);
uint64_t helper_be_ldq_mmu(uint32_t env, uint32_t addr, uint32_t oi, uintptr_t retaddr);

void helper_ret_stb_mmu(uint32_t env, uint32_t addr, uint8_t val, uint32_t oi, uintptr_t retaddr);
void helper_le_stw_mmu(uint32_t env, uint32_t addr, uint16_t val, uint32_t oi, uintptr_t retaddr);
void helper_le_stl_mmu(uint32_t env, uint32_t addr, uint32_t val, uint32_t oi, uintptr_t retaddr);
void helper_le_stq_mmu(uint32_t env, uint32_t addr, uint64_t val, uint32_t oi, uintptr_t retaddr);
void helper_be_stw_mmu(uint32_t env, uint32_t addr, uint16_t val, uint32_t oi, uintptr_t retaddr);
void helper_be_stl_mmu(uint32_t env, uint32_t addr, uint32_t val, uint32_t oi, uintptr_t retaddr);
void helper_be_stq_mmu(uint32_t env, uint32_t addr, uint64_t val, uint32_t oi, uintptr_t retaddr);

static uint32_t temp_ret;
static uint32_t set_temp_ret(uint64_t x)
{
  temp_ret = (uint32_t)(x >> 32);
  return (uint32_t)x;
}
static uint32_t get_temp_ret()
{
  return temp_ret;
}

}

struct QemuExternalInterface : ModuleInstance::ExternalInterface {
  virtual void importGlobals(TrivialGlobalManager& globals, Module& wasm) override {}
  virtual Literal callImport(Function* import, LiteralList& xs) override
  {
    static Name n_call_helper("call_helper");

    static Name n_helper_ret_ldub_mmu("helper_ret_ldub_mmu");
    static Name n_helper_le_lduw_mmu("helper_le_lduw_mmu");
    static Name n_helper_le_ldul_mmu("helper_le_ldul_mmu");
    static Name n_helper_le_ldq_mmu("helper_le_ldq_mmu");
    static Name n_helper_be_lduw_mmu("helper_be_lduw_mmu");
    static Name n_helper_be_ldul_mmu("helper_be_ldul_mmu");
    static Name n_helper_be_ldq_mmu("helper_be_ldq_mmu");

    static Name n_get_temp_ret("get_temp_ret");

    static Name n_helper_ret_stb_mmu("helper_ret_stb_mmu");
    static Name n_helper_le_stw_mmu("helper_le_stw_mmu");
    static Name n_helper_le_stl_mmu("helper_le_stl_mmu");
    static Name n_helper_le_stq_mmu("helper_le_stq_mmu");
    static Name n_helper_be_stw_mmu("helper_be_stw_mmu");
    static Name n_helper_be_stl_mmu("helper_be_stl_mmu");
    static Name n_helper_be_stq_mmu("helper_be_stq_mmu");

    Name name = import->name;
    if (name == n_call_helper) {
      return Literal(set_temp_ret(call_helper((helper_func)xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32(), xs[4].geti32(), xs[5].geti32(), xs[6].geti32(), xs[7].geti32(), xs[8].geti32(), xs[9].geti32(), xs[10].geti32(), xs[11].geti32(), xs[12].geti32())));

    } else if (name == n_helper_ret_ldub_mmu) {
      return     Literal(helper_ret_ldub_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32()));

    } else if (name == n_helper_le_lduw_mmu) {
      return     Literal(helper_le_lduw_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32()));
    } else if (name == n_helper_le_ldul_mmu) {
      return     Literal(helper_le_ldul_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32()));
    } else if (name == n_helper_le_ldq_mmu) {
      return Literal(set_temp_ret(helper_le_ldq_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32())));

    } else if (name == n_helper_be_lduw_mmu) {
      return     Literal(helper_be_lduw_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32()));
    } else if (name == n_helper_be_ldul_mmu) {
      return     Literal(helper_be_ldul_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32()));
    } else if (name == n_helper_be_ldq_mmu) {
      return Literal(set_temp_ret(helper_be_ldq_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32())));

    } else if (name == n_get_temp_ret) {
      return   Literal(get_temp_ret());

    } else if (name == n_helper_ret_stb_mmu) {
      helper_ret_stb_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32(), xs[4].geti32());

    } else if (name == n_helper_le_stw_mmu) {
      helper_le_stw_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32(), xs[4].geti32());
    } else if (name == n_helper_le_stl_mmu) {
      helper_le_stl_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32(), xs[4].geti32());
    } else if (name == n_helper_le_stq_mmu) {
      helper_le_stq_mmu(xs[0].geti32(), xs[1].geti32(), ((uint32_t)xs[2].geti32()) | (((uint64_t)xs[3].geti32()) << 32), xs[4].geti32(), xs[5].geti32());

    } else if (name == n_helper_be_stw_mmu) {
      helper_be_stw_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32(), xs[4].geti32());
    } else if (name == n_helper_be_stl_mmu) {
      helper_be_stl_mmu(xs[0].geti32(), xs[1].geti32(), xs[2].geti32(), xs[3].geti32(), xs[4].geti32());
    } else if (name == n_helper_be_stq_mmu) {
      helper_be_stq_mmu(xs[0].geti32(), xs[1].geti32(), ((uint32_t)xs[2].geti32()) | (((uint64_t)xs[3].geti32()) << 32), xs[4].geti32(), xs[5].geti32());
    } else {
      WASM_UNREACHABLE();
    }
    return Literal();
  }
  virtual Literal callTable(Index index, LiteralList& xs, Type result, ModuleInstance& instance) override { trap("unexpected callTable"); return Literal(); }

  virtual void growMemory(Address oldSize, Address newSize) override { trap("unexpected growMemory"); }

  virtual void trap(const char* why) override
  {
    fprintf(stderr, "TCG trap: %s\n", why);
    abort();
  }

  virtual int8_t load8s(Address addr)  override { return ldsb_p((void *)addr.addr); }
  virtual uint8_t load8u(Address addr) override { return ldub_p((void *)addr.addr); }
  virtual int16_t load16s(Address addr)  override { return ldsw_he_p((void *)addr.addr); }
  virtual uint16_t load16u(Address addr) override { return lduw_he_p((void *)addr.addr); }
  virtual int32_t load32s(Address addr)  override { return ldl_he_p((void *)addr.addr); }
  virtual uint32_t load32u(Address addr) override { return ldl_he_p((void *)addr.addr); }
  virtual int64_t load64s(Address addr)  override { return ldq_he_p((void *)addr.addr); }
  virtual uint64_t load64u(Address addr) override { return ldq_he_p((void *)addr.addr); }

  virtual void store8(Address addr, int8_t value)   override { return stb_p((void *)addr.addr, value); }
  virtual void store16(Address addr, int16_t value) override { return stw_he_p((void *)addr.addr, value); }
  virtual void store32(Address addr, int32_t value) override { return stl_he_p((void *)addr.addr, value); }
  virtual void store64(Address addr, int64_t value) override { return stq_he_p((void *)addr.addr, value); }
};

extern "C" void create_invoker(void)
{
  BinaryenModuleRef invoker = BinaryenModuleCreate();

  // Initialize types
  BinaryenType many_int32s[] = {
    BinaryenTypeInt32(),
    BinaryenTypeInt32(),
    BinaryenTypeInt32()
  };
  BinaryenAddFunctionType(
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
  BinaryenAddFunction(
    invoker,
    "invoke_tb", invoker_type,
    NULL, 0, function_body
  );

  BinaryenAddTableImport(invoker, NULL, "env", "tb_funcs");
  BinaryenAddFunctionExport(invoker, "invoke_tb", "invoke_tb");


  static char buf[4096];
  BinaryenModuleValidate(invoker);
  BinaryenModuleOptimize(invoker);
  int sz = BinaryenModuleWrite(invoker, buf, sizeof(buf));
  BinaryenModuleDispose(invoker);

  invoke_tb = (uintptr_t (*)(int, void *, uintptr_t)) EM_ASM_INT({
    var module = new WebAssembly.Module(new Uint8Array(wasmMemory.buffer, $0, $1));
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

static QemuExternalInterface interface;

extern "C" void *prepare_module(BinaryenModuleRef MODULE, BinaryenExpressionRef expr)
{
    BinaryenAddFunction(MODULE, "tb_fun", tb_func_type, func_locals, FUNC_LOCALS_COUNT, expr);
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
    BinaryenAddFunctionImport(MODULE, "helper_le_stq_mmu", "env", "helper_le_stq_mmu"  , st64_type);
    BinaryenAddFunctionImport(MODULE, "helper_be_stw_mmu", "env", "helper_be_stw_mmu"  , st32_type);
    BinaryenAddFunctionImport(MODULE, "helper_be_stl_mmu", "env", "helper_be_stl_mmu"  , st32_type);
    BinaryenAddFunctionImport(MODULE, "helper_be_stq_mmu", "env", "helper_be_stq_mmu"  , st64_type);

    BinaryenSetMemory(MODULE, (1 << 15) - 1, -1, NULL, NULL, NULL, NULL, NULL, 0, 0);
    BinaryenAddMemoryImport(MODULE, NULL, "env", "memory", 0);
    BinaryenAddTableImport(MODULE, NULL, "env", "tb_funcs");


//     assert (BinaryenModuleValidate(MODULE));
    BinaryenModuleOptimize(MODULE);

    return (void *)(new ModuleInstance(*(Module*)MODULE, &interface));
}

extern "C" void delete_instance(TranslationBlock *tb, void *_wi)
{
  if (_wi) {
    BinaryenModuleRef wasm = &((ModuleInstance *)(_wi))->wasm;
    delete (ModuleInstance *)_wi;
    BinaryenModuleDispose(wasm);
  } else {
    EM_ASM({ delete CompiledTBTable[$0]; }, get_fptr(tb));
  }
}

extern "C" uintptr_t interpret_module(void *_module, void *env, uintptr_t sp_value)
{
  static Name tb_fun("tb_fun");
  LiteralList args = { Literal((uint32_t)env), Literal((uint32_t)sp_value) };
  return ((ModuleInstance *)_module)->callExport(tb_fun, args).geti32();
}

extern "C" void compile_module(TranslationBlock *tb, void *_wi)
{
  BinaryenModuleRef MODULE = &((ModuleInstance*)_wi)->wasm;
  delete (ModuleInstance*)_wi;

  static char buf[1 << 20];
  BinaryenModuleOptimize(MODULE);
  BinaryenSetMemory(MODULE, 0, -1, NULL, NULL, NULL, NULL, NULL, 0, 0);
  int sz = BinaryenModuleWrite(MODULE, buf, sizeof(buf));
  BinaryenModuleDispose(MODULE);

  EM_ASM({
      var module = new WebAssembly.Module(new Uint8Array(wasmMemory.buffer, $0, $1));
      var fptr = $2;
      var instance = new WebAssembly.Instance(module, {
          'env': {
              'memory': wasmMemory,
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
  }, buf, sz, get_fptr(tb));
}
