#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <iostream>
#include <string>
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
#include "llvm/Object/ObjectFile.h"
#include "YTTypes.h"
using namespace llvm;
using namespace object;

void func(TRow row, TValue* result)
{
  TValue* values = (TValue*)(row + 1);
  result->Data.Int64 = values[0].Data.Int64 + values[1].Data.Int64;
}

void addFuncIRToModule(Module* module)
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
}

/* UDFs using cached .so files */

class SharedObjectMemoryManager : public SectionMemoryManager
{
  SharedObjectMemoryManager(const SharedObjectMemoryManager&) LLVM_DELETED_FUNCTION;
  void operator=(const SharedObjectMemoryManager&) LLVM_DELETED_FUNCTION;

public:
  SharedObjectMemoryManager() {}
  virtual ~SharedObjectMemoryManager() {}

  virtual uint64_t getSymbolAddress(const std::string &Name);

private:
  ExecutionEngine* udfEngine = NULL;
};

uint64_t SharedObjectMemoryManager::getSymbolAddress(const std::string &Name)
{
  // Gets called when "call" cannot find the function
  uint64_t addr = SectionMemoryManager::getSymbolAddress(Name);
  if (!addr) {
    if (!udfEngine) {
      Module* udfModule = new Module("udf", getGlobalContext());
      udfEngine = EngineBuilder(udfModule).setUseMCJIT(true).create();

      std::string unmangledName = Name.front() == '_' ? Name.substr(1) : Name;
      ErrorOr<std::unique_ptr<MemoryBuffer>> buffer = MemoryBuffer::getFile(unmangledName + ".so");
      ErrorOr<std::unique_ptr<ObjectFile>> object = ObjectFile::createObjectFile(buffer.get());

      udfEngine->addObjectFile(move(object.get()));
      udfEngine->finalizeObject();
    }

    addr = (uint64_t)udfEngine->getPointerToNamedFunction(Name);
  }
  return addr;
}

ExecutionEngine* getObjectEngine(Module* module) {
  return EngineBuilder(module)
    .setUseMCJIT(true)
    .setMCJITMemoryManager(new SharedObjectMemoryManager())
    .create();
}

/* UDFs from LLVM IR */

ExecutionEngine* getIREngine(Module* module) {
  return EngineBuilder(module)
    .setUseMCJIT(true)
    .create();
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
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  Module* module = new Module("addition_func", getGlobalContext());
  addFuncIRToModule(module);
  ExecutionEngine* engine = getObjectEngine(module);

  engine->finalizeObject();
  void* funcPtr = engine->getPointerToNamedFunction("func");

  void(*llvmfunc)(TRow, TValue*) = (void (*)(TRow, TValue*))funcPtr;
  llvmfunc(row, &(values[2]));

  printf("Func execution: %ld (expected 1200)\n", (long)values[2].Data.Int64);
}
