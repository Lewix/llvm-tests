#include "llvm/IR/Verifier.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/TargetSelect.h"
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <iostream>
#include <string>
using namespace llvm;

typedef int64_t i64;
typedef int32_t i32;
typedef int8_t i8;
typedef uint64_t ui64;
typedef uint32_t ui32;
typedef uint8_t ui8;

/* YT data types */

enum EValueType {
  Null,
  Int64,
  Uint64,
  Double,
  Boolean,
  String,
  Any // Don’t bother right now.
};

struct TValue {
  i8 Id; // Column name.
  i8 Type; // Column type (EValueType).
  i32 Length; // For string-like values this fields contains size of variable part.
  union {
    i64 Int64;
    ui64 Uint64;
    double Double;
    bool Boolean;
    const char* String;
  } Data;
};

struct TRowHeader
{
  int Count; // Number of values in a row.
  int Padding; // Pads TRowHeader to be 16-byte.
};

using TRow = TRowHeader*;

/* LLMV types for YT data types */

namespace llvm {
template<bool xcompile> class TypeBuilder<TValue, xcompile> {
 public:
  static StructType* get(LLVMContext &context) {
    return StructType::get(
      TypeBuilder<types::i<16>, xcompile>::get(context),
      TypeBuilder<types::i<16>, xcompile>::get(context),
      TypeBuilder<types::i<32>, xcompile>::get(context),
      TypeBuilder<types::i<64>, xcompile>::get(context),
      NULL);
  }
};

template<bool xcompile> class TypeBuilder<TRowHeader, xcompile> {
 public:
  static StructType* get(LLVMContext &context) {
    return StructType::get(
      TypeBuilder<types::i<32>, xcompile>::get(context),
      TypeBuilder<types::i<32>, xcompile>::get(context),
      NULL);
  }
};
}

/* Exercise 1 */

void func(TRow row, TValue* result)
{
  TValue* values = (TValue*)(row + 1);
  result->Data.Int64 = values[0].Data.Int64 + values[1].Data.Int64;
}

void funcIR(Module* module)
{
  LLVMContext &context = getGlobalContext();
  IRBuilder<> builder(context);

  FunctionType* funTp = TypeBuilder<void(TRow, TValue*), true>::get(context);
  Type* tvalueTp = TypeBuilder<TValue, true>::get(context);
  Type* tvaluePtrTp = TypeBuilder<TValue*, true>::get(context);

  Function* function = Function::Create(funTp, Function::ExternalLinkage, "func", module);
  Function* udf = Function::Create(funTp, Function::ExternalLinkage, "myudf", module);
  BasicBlock* body = BasicBlock::Create(context, "entry", function);
  builder.SetInsertPoint(body);

  Function::arg_iterator args = function->arg_begin();
  Argument* rowArg = args;
  rowArg->setName("row");
  args++;
  Argument* resultArg = args;
  resultArg->setName("result");

  // TValue* values = (TValue*)(row + 1)
  Value* rowIncPtr = builder.CreateConstInBoundsGEP1_32(rowArg, 1);
  Value* valuesPtr = builder.CreatePointerCast(rowIncPtr, tvaluePtrTp, "values");

  // i64 value0Data = values[0].Data.Int64
  Value* value0DataPtr = builder.CreateConstInBoundsGEP2_32(valuesPtr, 0, 3);
  Value* value0Data = builder.CreateLoad(value0DataPtr, "value0Data");

  // i64 value1Data = values[1].Data.Int64
  Value* value1DataPtr = builder.CreateConstInBoundsGEP2_32(valuesPtr, 1, 3);
  Value* value1Data = builder.CreateLoad(value1DataPtr, "value1Data");

  // i64 valueSum = value0Data + value1Data
  Value* valueSum = builder.CreateAdd(value0Data, value1Data, "valueSum");

  // Now call "func" from the .so file and add this to the result
  // { i16, 16, i32, i64 } udfResult = alloca t_value
  Value* udfResult = builder.CreateAlloca(tvalueTp);
  // call myudf(row result) 
  builder.CreateCall2(udf, rowArg, udfResult);
  // i64 udfData = result->Data.Int64
  Value* udfDataPtr = builder.CreateConstInBoundsGEP2_32(udfResult, 0, 3);
  Value* udfData = builder.CreateLoad(udfDataPtr, "udfData");
  // i64 udfSum = valueSum + udfData
  Value* udfSum = builder.CreateAdd(valueSum, udfData, "udfSum");

  // result->Data.Int64 = udfSum;
  Value* resultDataPtr = builder.CreateConstInBoundsGEP2_32(resultArg, 0, 3);
  builder.CreateStore(udfSum, resultDataPtr);

  builder.CreateRetVoid();

  verifyFunction(*function);

  module->dump();
}

class SharedObjectMemoryManager : public SectionMemoryManager
{
  SharedObjectMemoryManager(const SharedObjectMemoryManager&) LLVM_DELETED_FUNCTION;
  void operator=(const SharedObjectMemoryManager&) LLVM_DELETED_FUNCTION;

public:
  SharedObjectMemoryManager() {}
  virtual ~SharedObjectMemoryManager() {}

  virtual uint64_t getSymbolAddress(const std::string &Name);
};

uint64_t SharedObjectMemoryManager::getSymbolAddress(const std::string &Name)
{
  // Gets called when "call" cannot find the function
  uint64_t addr = SectionMemoryManager::getSymbolAddress(Name);
  if (!addr) {
    void* dynamicLib = dlopen("func.so", RTLD_LAZY);
    return (uint64_t)dlsym(dynamicLib, "func");
  }
  return addr;
}

// Gets called when getPointerToNamedFunction cannot find the function
void* functionCreator(const std::string& name) {
  void* dynamicLib = dlopen("func.so", RTLD_LAZY);
  return dlsym(dynamicLib, "func");
}

int main(int argc, char** argv)
{
  char buffer[sizeof(TRowHeader) + 3 * sizeof(TValue)];
  TRowHeader* row = (TRowHeader*)buffer;
  TValue* values = (TValue*)(row + 1);
  row->Count = 3;
  row->Padding = 0;
  values[0] = { 0, EValueType::Int64, 0, { 100 } }; // int64(100)
  values[1] = { 0, EValueType::Int64, 0, { 500 } }; // int64(500)
  values[2] = { 0, EValueType::Int64, 0, { 0 } }; // int64(0)

  // Run func
  func(row, &values[2]);
  printf("func execution: %ld\n", (long)values[2].Data.Int64);
  values[2] = { 0, EValueType::Int64, 0, { 0 } }; // int64(0)

  // Run JIT-compiled func
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  Module* module = new Module("addition_func", getGlobalContext());
  funcIR(module);
  ExecutionEngine* engine = EngineBuilder(module)
    .setUseMCJIT(true)
    .setMCJITMemoryManager(new SharedObjectMemoryManager())
    .create();

  engine->InstallLazyFunctionCreator(functionCreator);
  engine->finalizeObject();
  void* funcPtr = engine->getPointerToNamedFunction("func");

  void(*llvmfunc)(TRow, TValue*) = (void (*)(TRow, TValue*))funcPtr;
  llvmfunc(row, &(values[2]));

  printf("JIT-compiled execution: %ld\n", (long)values[2].Data.Int64);
  values[2] = { 0, EValueType::Int64, 0, { 0 } }; // int64(0)

  // Run func from .so file
  void* dynamicLib = dlopen("func.so", RTLD_LAZY);
  void* funcSym = dlsym(dynamicLib, "func");
  void(*sofunc)(TRow, TValue*) = (void (*)(TRow, TValue*))funcSym;
  sofunc(row, &(values[2]));

  printf(".so execution: %ld\n", (long)values[2].Data.Int64);
}
