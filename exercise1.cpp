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
using namespace llvm;

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
  short Id; // Column name.
  short Type; // Column type (EValueType).
  int Length; // For string-like values this fields contains size of variable part.
  union {
    long Int64;
    unsigned long Uint64;
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

Module* funcIR(TRow row, TValue* result)
{
  LLVMContext &context = getGlobalContext();
  Module* module = new Module("addition func", context);
  IRBuilder<> builder(context);

  FunctionType* funTp = TypeBuilder<void(TRow, TValue*), true>::get(context);
  Type* i64Tp = TypeBuilder<types::i<64>, true>::get(context);
  Type* i32Tp = TypeBuilder<types::i<32>, true>::get(context);
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
  Value* one = Constant::getIntegerValue(i64Tp, APInt(64, 1));
  Value* rowIncPtr = builder.CreateInBoundsGEP(rowArg, one);
  Instruction::CastOps castOp = CastInst::getCastOpcode(rowIncPtr, false, tvaluePtrTp, false);
  Value* valuesPtr = builder.CreateCast(castOp, rowIncPtr, tvaluePtrTp, "values");

  // i64 value0Data = values[0].Data.Int64
  Value* indices[2] = {
    Constant::getIntegerValue(i64Tp, APInt(64, 0)),
    Constant::getIntegerValue(i32Tp, APInt(32, 3))
  };
  Value* value0DataPtr =
    builder.CreateInBoundsGEP(valuesPtr, ArrayRef<Value*>(indices, 2));
  Value* value0Data = builder.CreateLoad(value0DataPtr, "value0Data");

  // i64 value1Data = values[1].Data.Int64
  indices[0] = Constant::getIntegerValue(i64Tp, APInt(64, 1));
  Value* value1DataPtr =
    builder.CreateInBoundsGEP(valuesPtr, ArrayRef<Value*>(indices, 2));
  Value* value1Data = builder.CreateLoad(value1DataPtr, "value1Data");

  // i64 valueSum = value0Data + value1Data
  Value* valueSum = builder.CreateAdd(value0Data, value1Data, "valueSum");

  // result->Data.Int64 = resultSum;
  indices[0] = Constant::getIntegerValue(i64Tp, APInt(64, 0));
  Value* resultDataPtr =
    builder.CreateInBoundsGEP(resultArg, ArrayRef<Value*>(indices, 2));
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
  Module* module = funcIR(row, &values[2]);
  ExecutionEngine* engine = EngineBuilder(module).create();
  void* funcPtr = engine->getPointerToFunction(module->getFunction("func"));

  void(*llvmfunc)(TRow, TValue*) = (void (*)(TRow, TValue*))funcPtr;
  llvmfunc(row, &(values[2]));

  printf("%ld\n", values[2].Data.Int64);
}
