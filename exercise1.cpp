#include "llvm/IR/Verifier.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/TargetSelect.h"
#include <stdio.h>
#include <stdint.h>
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

Module* funcIR()
{
  LLVMContext &context = getGlobalContext();
  Module* module = new Module("addition func", context);
  IRBuilder<> builder(context);

  FunctionType* funTp = TypeBuilder<void(TRow, TValue*), true>::get(context);
  Type* tvaluePtrTp = TypeBuilder<TValue*, true>::get(context);

  Function* function = Function::Create(funTp, Function::ExternalLinkage, "func", module);
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

  // result->Data.Int64 = resultSum;
  Value* resultDataPtr = builder.CreateConstInBoundsGEP2_32(resultArg, 0, 3);
  builder.CreateStore(valueSum, resultDataPtr);

  builder.CreateRetVoid();

  verifyFunction(*function);

  module->dump();

  return module;
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

  InitializeNativeTarget();
  Module* module = funcIR();
  ExecutionEngine* engine = EngineBuilder(module).create();
  void* funcPtr = engine->getPointerToFunction(module->getFunction("func"));

  void(*llvmfunc)(TRow, TValue*) = (void (*)(TRow, TValue*))funcPtr;
  llvmfunc(row, &(values[2]));

  printf("%ld\n", (long)values[2].Data.Int64);
}
