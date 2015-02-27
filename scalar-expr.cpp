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
#include "YTTypes.h"
#include "TExpression.h"
#include "FunctionRegistry.h"
#include "LLVMCodegen.h"

Value* LLVMCodegen::generate(std::shared_ptr<TExpression> expr, IRBuilder<> builder)
{
  LLVMContext& context = getGlobalContext();
  if (expr->As<TLiteralExpression>()) {
    TLiteralExpression* literalExpr = expr->As<TLiteralExpression>();
    switch (expr->Type) {
      case EValueType::Int64:
      case EValueType::Uint64: {
        i64 literal = literalExpr->Value->Data.Int64;
        return builder.getInt64(literal);
      }
      case EValueType::Double: {
        double literal = literalExpr->Value->Data.Double;
        return ConstantFP::get(Type::getDoubleTy(context), literal);
      }
      case EValueType::Boolean: {
        bool literal = literalExpr->Value->Data.Boolean;
        return builder.getInt1(literal);
      }
      case EValueType::String: {
        //TODO
      }
      default:
        return NULL;
    }
  } else if (expr->As<TBinaryOpExpression>()) {
    TBinaryOpExpression* binOpExpr = expr->As<TBinaryOpExpression>();
    Value* lhs = generate(binOpExpr->Lhs, builder);
    Value* rhs = generate(binOpExpr->Rhs, builder);
    auto binOpSig = registry->GetFunction(
      binOpExpr->Opcode,
      typeOf(binOpExpr));
    FunctionsToEmit.push_back(binOpSig);
    auto function = GetLLVMFunction(binOpSig, ExpressionModule);
    return builder.CreateCall2(function, lhs, rhs);
  }
  //TODO: TFunctionExpression case

  return NULL;
}

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

void testInt()
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

void testDouble()
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

int main(int argc, char** argv)
{
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();


  std::vector<EValueType> plusDoubleArgTypes;
  plusDoubleArgTypes.push_back(EValueType::Double);
  plusDoubleArgTypes.push_back(EValueType::Double);
  registry->AddFunction(FunctionSignature(
    FunctionRegistry::getOperatorName(EBinaryOp::Plus),
    plusDoubleArgTypes,
    EValueType::Double,
    emitPlusDouble));

  std::vector<EValueType> plusIntArgTypes;
  plusIntArgTypes.push_back(EValueType::Int64);
  plusIntArgTypes.push_back(EValueType::Int64);
  registry->AddFunction(FunctionSignature(
    FunctionRegistry::getOperatorName(EBinaryOp::Plus),
    plusIntArgTypes,
    EValueType::Int64,
    emitPlusInt));


  testInt();
  testDouble();

  // Other tests:
  // 1.5 + 2.0 + 3.0 = 6.5
  // (2 * 4) + 3 = 11
}
