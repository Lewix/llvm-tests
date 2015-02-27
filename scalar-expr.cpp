#include <iostream>
#include <utility>
#include <string>
#include <functional>
#include <unordered_map>
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Support/SourceMgr.h"
#include "LLVMCodegen.h"
#include "llvm/IRReader/IRReader.h"
using namespace TExpressionTyper;
using namespace llvm;

Module* emitPlusInt(IRBuilder<>& builder)
{
  LLVMContext &context = getGlobalContext();
  std::string name = FunctionRegistry::getOperatorName(EBinaryOp::Plus);
  const FunctionSignature* plusSig = registry->GetFunction(
    name,
    EValueType::Int64);
  FunctionType* plusTp = LLVMCodegen::getLLVMType(plusSig);
  Module* module = new Module(name, context);

  Function* plusFunction = Function::Create(
    plusTp,
    Function::ExternalLinkage,
    FunctionRegistry::getOperatorName(EBinaryOp::Plus),
    module);

  Function::arg_iterator args = plusFunction->arg_begin();
  Argument* arg1 = args;
  args++;
  Argument* arg2 = args;

  BasicBlock* body = BasicBlock::Create(context, "entry", plusFunction);
  builder.SetInsertPoint(body);

  Value* sum = builder.CreateAdd(arg1, arg2);
  builder.CreateRet(sum);

  verifyFunction(*plusFunction);

  return module;
}

Module* emitPlusDouble(IRBuilder<>& builder)
{
  LLVMContext &context = getGlobalContext();
  std::string name = FunctionRegistry::getOperatorName(EBinaryOp::Plus);
  const FunctionSignature* plusSig = registry->GetFunction(
    name,
    EValueType::Double);
  FunctionType* plusTp = LLVMCodegen::getLLVMType(plusSig);
  Module* module = new Module(name, context);

  Function* plusFunction = Function::Create(
    plusTp,
    Function::ExternalLinkage,
    FunctionRegistry::getOperatorName(EBinaryOp::Plus),
    module);

  Function::arg_iterator args = plusFunction->arg_begin();
  Argument* arg1 = args;
  args++;
  Argument* arg2 = args;

  BasicBlock* body = BasicBlock::Create(context, "entry", plusFunction);
  builder.SetInsertPoint(body);

  Value* sum = builder.CreateFAdd(arg1, arg2);
  builder.CreateRet(sum);

  verifyFunction(*plusFunction);

  return module;
}

Module* emitMultiplyInt(IRBuilder<>& builder)
{
  LLVMContext &context = getGlobalContext();
  std::string name = FunctionRegistry::getOperatorName(EBinaryOp::Multiply);
  const FunctionSignature* multiplySig = registry->GetFunction(
    name,
    EValueType::Int64);
  FunctionType* multiplyTp = LLVMCodegen::getLLVMType(multiplySig);
  Module* module = new Module(name, context);

  Function* multiplyFunction = Function::Create(
    multiplyTp,
    Function::ExternalLinkage,
    FunctionRegistry::getOperatorName(EBinaryOp::Multiply),
    module);

  Function::arg_iterator args = multiplyFunction->arg_begin();
  Argument* arg1 = args;
  args++;
  Argument* arg2 = args;

  BasicBlock* body = BasicBlock::Create(context, "entry", multiplyFunction);
  builder.SetInsertPoint(body);

  Value* product = builder.CreateMul(arg1, arg2);
  builder.CreateRet(product);

  verifyFunction(*multiplyFunction);

  return module;
}

Module* emitExp(IRBuilder<>& builder)
{
  SMDiagnostic diag;
  return ParseIRFile("exp.o", diag, getGlobalContext());
}

void testPlusInt()
{
  // 1 + 2 + 3
  std::shared_ptr<TValue> one = std::make_shared<TValue>();
  one->Id = 0; one->Type = EValueType::Int64; one->Length = 0;
  one->Data = { 1 };
  std::shared_ptr<TValue> two = std::make_shared<TValue>();
  two->Id = 0; two->Type = EValueType::Int64; two->Length = 0;
  two->Data = { 2 };
  std::shared_ptr<TValue> three = std::make_shared<TValue>();
  three->Id = 0; three->Type = EValueType::Int64; three->Length = 0;
  three->Data = { 3 };

  TConstExpressionPtr oneExpr =
    std::make_shared<TLiteralExpression>(EValueType::Int64, one);
  TConstExpressionPtr twoExpr =
    std::make_shared<TLiteralExpression>(EValueType::Int64, two);
  TConstExpressionPtr threeExpr =
    std::make_shared<TLiteralExpression>(EValueType::Int64, three);

  std::shared_ptr<TExpression> expr1 =
    std::make_shared<TBinaryOpExpression>(
      EValueType::Null,
      EBinaryOp::Plus,
      oneExpr,
      std::make_shared<TBinaryOpExpression>(
        EValueType::Null,
        EBinaryOp::Plus,
        twoExpr,
        threeExpr));

  LLVMCodegen codegen;
  Module* module = codegen.GetExpressionModule(expr1);

  ExecutionEngine* engine = EngineBuilder(module)
    .setUseMCJIT(true)
    .create();
  engine->finalizeObject();

  void* exprFunPtr = engine->getPointerToNamedFunction("expr");
  int(*exprFun)(void) = (int(*)(void))exprFunPtr;
  std::cout << "1 + 2 + 3 = " << exprFun() << std::endl;
}

void testPlusDouble()
{
  std::shared_ptr<TValue> one = std::make_shared<TValue>();
  one->Id = 0; one->Type = EValueType::Double; one->Length = 0;
  one->Data.Double = { 1.5 };
  std::shared_ptr<TValue> two = std::make_shared<TValue>();
  two->Id = 0; two->Type = EValueType::Double; two->Length = 0;
  two->Data.Double = { 2.0 };
  std::shared_ptr<TValue> three = std::make_shared<TValue>();
  three->Id = 0; three->Type = EValueType::Double; three->Length = 0;
  three->Data.Double = { 3.0 };

  TConstExpressionPtr oneExpr =
    std::make_shared<TLiteralExpression>(EValueType::Double, one);
  TConstExpressionPtr twoExpr =
    std::make_shared<TLiteralExpression>(EValueType::Double, two);
  TConstExpressionPtr threeExpr =
    std::make_shared<TLiteralExpression>(EValueType::Double, three);

  std::shared_ptr<TExpression> expr =
    std::make_shared<TBinaryOpExpression>(
      EValueType::Null,
      EBinaryOp::Plus,
      oneExpr,
      std::make_shared<TBinaryOpExpression>(
        EValueType::Null,
        EBinaryOp::Plus,
        twoExpr,
        threeExpr));

  LLVMCodegen codegen;
  Module* module = codegen.GetExpressionModule(expr);

  ExecutionEngine* engine = EngineBuilder(module)
    .setUseMCJIT(true)
    .create();
  engine->finalizeObject();

  void* exprFunPtr = engine->getPointerToNamedFunction("expr");
  double(*exprFun)(void) = (double(*)(void))exprFunPtr;
  std::cout << "1.5 + 2.0 + 3.0 = " << exprFun() << std::endl;
}

void testMultiplyInt()
{
  std::shared_ptr<TValue> two = std::make_shared<TValue>();
  two->Id = 0; two->Type = EValueType::Int64; two->Length = 0;
  two->Data = { 2 };
  std::shared_ptr<TValue> four = std::make_shared<TValue>();
  four->Id = 0; four->Type = EValueType::Int64; four->Length = 0;
  four->Data = { 4 };
  std::shared_ptr<TValue> three = std::make_shared<TValue>();
  three->Id = 0; three->Type = EValueType::Int64; three->Length = 0;
  three->Data = { 3 };

  TConstExpressionPtr twoExpr =
    std::make_shared<TLiteralExpression>(EValueType::Int64, two);
  TConstExpressionPtr fourExpr =
    std::make_shared<TLiteralExpression>(EValueType::Int64, four);
  TConstExpressionPtr threeExpr =
    std::make_shared<TLiteralExpression>(EValueType::Int64, three);

  std::shared_ptr<TExpression> expr =
    std::make_shared<TBinaryOpExpression>(
      EValueType::Null,
      EBinaryOp::Plus,
      std::make_shared<TBinaryOpExpression>(
        EValueType::Null,
        EBinaryOp::Multiply,
        twoExpr,
        fourExpr),
      threeExpr);

  LLVMCodegen codegen;
  Module* module = codegen.GetExpressionModule(expr);

  ExecutionEngine* engine = EngineBuilder(module)
    .setUseMCJIT(true)
    .create();
  engine->finalizeObject();

  void* exprFunPtr = engine->getPointerToNamedFunction("expr");
  int(*exprFun)(void) = (int(*)(void))exprFunPtr;
  std::cout << "(2 * 4) + 3 = " << exprFun() << std::endl;
}

void testUdf()
{
  std::shared_ptr<TValue> two = std::make_shared<TValue>();
  two->Id = 0; two->Type = EValueType::Int64; two->Length = 0;
  two->Data = { 2 };
  std::shared_ptr<TValue> four = std::make_shared<TValue>();
  four->Id = 0; four->Type = EValueType::Int64; four->Length = 0;
  four->Data = { 4 };

  TConstExpressionPtr twoExpr =
    std::make_shared<TLiteralExpression>(EValueType::Int64, two);
  TConstExpressionPtr fourExpr =
    std::make_shared<TLiteralExpression>(EValueType::Int64, four);
  
  TArguments args({ twoExpr, fourExpr });
  std::shared_ptr<TExpression> expr =
    std::make_shared<TFunctionExpression>(
      EValueType::Null,
      "_Z3expll",
      args);

  LLVMCodegen codegen;
  Module* module = codegen.GetExpressionModule(expr);

  ExecutionEngine* engine = EngineBuilder(module)
    .setUseMCJIT(true)
    .create();
  engine->finalizeObject();
  void* exprFunPtr = engine->getPointerToNamedFunction("expr");
  int(*exprFun)(void) = (int(*)(void))exprFunPtr;
  std::cout << "2 ^ 4 = " << exprFun() << std::endl;
}

int main(int argc, char** argv)
{
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();

  std::vector<EValueType> plusDoubleArgTypes({
    EValueType::Double, EValueType::Double
  });
  registry->AddFunction(FunctionSignature(
    FunctionRegistry::getOperatorName(EBinaryOp::Plus),
    plusDoubleArgTypes,
    EValueType::Double,
    emitPlusDouble));

  std::vector<EValueType> plusIntArgTypes({
    EValueType::Int64, EValueType::Int64
  });
  registry->AddFunction(FunctionSignature(
    FunctionRegistry::getOperatorName(EBinaryOp::Plus),
    plusIntArgTypes,
    EValueType::Int64,
    emitPlusInt));

  std::vector<EValueType> multiplyIntArgTypes({
    EValueType::Int64, EValueType::Int64
  });
  registry->AddFunction(FunctionSignature(
    FunctionRegistry::getOperatorName(EBinaryOp::Multiply),
    multiplyIntArgTypes,
    EValueType::Int64,
    emitMultiplyInt));

  std::vector<EValueType> expTypes({
    EValueType::Int64, EValueType::Int64
  });
  registry->AddFunction(FunctionSignature(
    "_Z3expll",
    expTypes,
    EValueType::Int64,
    emitExp));

  testPlusInt();
  testPlusDouble();
  testMultiplyInt();
  testUdf();
}
