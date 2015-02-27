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

EValueType typeOf(const TExpression* expr)
{
  if (expr->As<TLiteralExpression>()) {
    return expr->Type;

  } else if (expr->As<TBinaryOpExpression>()) {
    const TBinaryOpExpression* binOpExpr = expr->As<TBinaryOpExpression>();
    EValueType lhsType = typeOf(binOpExpr->Lhs.get());
    EValueType rhsType = typeOf(binOpExpr->Rhs.get());
    const FunctionSignature* signature =
      registry->GetFunction(binOpExpr->Opcode);
    if (signature->ArgumentTypes.size() == 2
        && signature->ArgumentTypes[0] == lhsType
        && signature->ArgumentTypes[1] == rhsType) {
      return signature->ReturnType;
    }

  } else if (expr->As<TFunctionExpression>()) {
    const TFunctionExpression* funExpr = expr->As<TFunctionExpression>();
    const FunctionSignature* signature =
      registry->GetFunction(funExpr->FunctionName);
    auto arg = funExpr->Arguments.begin();
    auto tpe = signature->ArgumentTypes.begin();
    for (; arg != funExpr->Arguments.end();
         arg++, tpe++) {
      if (typeOf(arg->get()) != *tpe) {
        return EValueType::Null;
      }
    }
    return signature->ReturnType;
  }

  return EValueType::Null;
}

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

int main(int argc, char** argv)
{
  std::vector<EValueType> plusArgTypes;
  plusArgTypes.push_back(EValueType::Int64);
  plusArgTypes.push_back(EValueType::Int64);
  registry->AddFunction(FunctionSignature(
    FunctionRegistry::getOperatorName(EValueType::Plus),
    plusArgTypes,
    EBinaryOp::Int64,
    emitPlusInt));

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
  
  std::cout << "Type of 1 + 2 + 3 is " << typeOf(expr.get()) << std::endl;

  LLVMCodegen codegen;
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();
  Module* module = codegen.GetExpressionModule(expr);
  module->dump();

  ExecutionEngine* engine = EngineBuilder(module)
    .setUseMCJIT(true)
    .create();
  engine->finalizeObject();

  void* exprFunPtr = engine->getPointerToNamedFunction("expr");
  int(*exprFun)(void) = (int(*)(void))exprFunPtr;
  std::cout << "Result of 1 + 2 + 3 is " << exprFun() << std::endl;
  

  // Other tests:
  // 1.5 + 2.0 + 3.0 = 6.5
  // (2 * 4) + 3 = 11
}
