#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"
#include "TExpressionTyper.h"
using namespace TExpressionTyper;

// Generates LLVM IR corresponding to given TExpressions
class LLVMCodegen {
public:
  LLVMCodegen() : ExpressionModule(new Module("expr", getGlobalContext())) { }

  Module* GetExpressionModule(std::shared_ptr<TExpression> expr);
  static Type* getLLVMType(EValueType type);
  static FunctionType* getLLVMType(const FunctionSignature* signature);
  static FunctionType* getLLVMType(EValueType resultType, std::vector<EValueType> argTypes);

private:
  Module* ExpressionModule;
  std::vector<const FunctionSignature*> FunctionsToEmit;

  Value* Generate(std::shared_ptr<TExpression> expr, IRBuilder<> builder);
  Value* GetLLVMFunction(const FunctionSignature* signature, Module* module);
};


Module* LLVMCodegen::GetExpressionModule(std::shared_ptr<TExpression> expr)
{
  LLVMContext& context = getGlobalContext();
  IRBuilder<> builder(context);
  FunctionType* funTp = getLLVMType(typeOf(expr.get()), std::vector<EValueType>());
  Function* exprFun = Function::Create(
    funTp,
    Function::ExternalLinkage,
    "expr",
    ExpressionModule);
  BasicBlock* body = BasicBlock::Create(context, "entry", exprFun);
  builder.SetInsertPoint(body);
  Value* result = Generate(expr, builder);
  builder.CreateRet(result);

  verifyFunction(*exprFun);

  Linker linker(ExpressionModule);
  for (auto functionSigs = FunctionsToEmit.begin();
       functionSigs != FunctionsToEmit.end();
       functionSigs++) {
    Module* functionModule = (*functionSigs)->IREmitter(builder);
    linker.linkInModule(functionModule, NULL);
  }

  return ExpressionModule;
}

Type* LLVMCodegen::getLLVMType(EValueType type)
{
  LLVMContext& context = getGlobalContext();
  switch (type) {
    case EValueType::Int64:
    case EValueType::Uint64:
      return Type::getInt64Ty(context);
    case EValueType::Double:
      return Type::getDoubleTy(context);
    case EValueType::Boolean:
      return Type::getInt1Ty(context);
    case EValueType::String:
      return Type::getInt8PtrTy(context);
    default:
      return NULL;
  }
}
FunctionType* LLVMCodegen::getLLVMType(const FunctionSignature* signature)
{
  return getLLVMType(signature->ReturnType, signature->ArgumentTypes);
}

FunctionType* LLVMCodegen::getLLVMType(EValueType resultType, std::vector<EValueType> argTypes)
{
  Type* result = getLLVMType(resultType);
  std::vector<Type*> args;
  auto argEValueTp = argTypes.begin();
  for (; argEValueTp != argTypes.end(); argEValueTp++) {
    args.push_back(getLLVMType(*argEValueTp));
  }
  return FunctionType::get(result, ArrayRef<Type*>(args), false);
}

Value* LLVMCodegen::Generate(std::shared_ptr<TExpression> expr, IRBuilder<> builder)
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
    Value* lhs = Generate(binOpExpr->Lhs, builder);
    Value* rhs = Generate(binOpExpr->Rhs, builder);
    auto binOpSig = registry->GetFunction(
      binOpExpr->Opcode,
      typeOf(binOpExpr));
    FunctionsToEmit.push_back(binOpSig);
    auto function = GetLLVMFunction(binOpSig, ExpressionModule);
    return builder.CreateCall2(function, lhs, rhs);
  } else if (expr->As<TFunctionExpression>()) {
    TFunctionExpression* funExpr = expr->As<TFunctionExpression>();
    std::vector<Value*> llvmArgs;
    for (auto args = funExpr->Arguments.begin();
         args != funExpr->Arguments.end();
         args++) {
      llvmArgs.push_back(Generate(*args, builder));
    }
    auto funSig = registry->GetFunction(
      funExpr->FunctionName,
      typeOf(funExpr));
    FunctionsToEmit.push_back(funSig);
    auto function = GetLLVMFunction(funSig, ExpressionModule);
    return builder.CreateCall(function, ArrayRef<Value*>(llvmArgs));
  }
  

  return NULL;
}

Value* LLVMCodegen::GetLLVMFunction(const FunctionSignature* signature, Module* module)
{
  return module->getOrInsertFunction(
    signature->Name,
    getLLVMType(signature));
}
