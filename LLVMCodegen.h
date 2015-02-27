#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"

EValueType typeOf(const TExpression* expr)
{
  if (expr->As<TLiteralExpression>()) {
    return expr->Type;

  } else if (expr->As<TBinaryOpExpression>()) {
    const TBinaryOpExpression* binOpExpr = expr->As<TBinaryOpExpression>();
    EValueType lhsType = typeOf(binOpExpr->Lhs.get());
    EValueType rhsType = typeOf(binOpExpr->Rhs.get());
    std::vector<EValueType> argTypes({ lhsType, rhsType });
    const FunctionSignature* signature = registry->GetFunction(
      binOpExpr->Opcode,
      EValueType::Null,
      &argTypes);
    if (signature->ArgumentTypes.size() == 2
        && signature->ArgumentTypes[0] == lhsType
        && signature->ArgumentTypes[1] == rhsType) {
      return signature->ReturnType;
    }

  } else if (expr->As<TFunctionExpression>()) {
    const TFunctionExpression* funExpr = expr->As<TFunctionExpression>();
    std::vector<EValueType> argTypes;
    for (auto args = funExpr->Arguments.begin();
         args != funExpr->Arguments.end();
         args++) {
      argTypes.push_back((*args)->Type);
    }
    const FunctionSignature* signature = registry->GetFunction(
      funExpr->FunctionName,
      EValueType::Null,
      &argTypes);
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
class LLVMCodegen {
public:
  LLVMCodegen() : ExpressionModule(new Module("expr", getGlobalContext())) { }

  Module* GetExpressionModule(std::shared_ptr<TExpression> expr)
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
    Value* result = generate(expr, builder);
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
  static Type* getLLVMType(EValueType type)
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
  static FunctionType* getLLVMType(const FunctionSignature* signature)
  {
    return getLLVMType(signature->ReturnType, signature->ArgumentTypes);
  }

  static FunctionType* getLLVMType(EValueType resultType, std::vector<EValueType> argTypes)
  {
    Type* result = getLLVMType(resultType);
    std::vector<Type*> args;
    auto argEValueTp = argTypes.begin();
    for (; argEValueTp != argTypes.end(); argEValueTp++) {
      args.push_back(getLLVMType(*argEValueTp));
    }
    return FunctionType::get(result, ArrayRef<Type*>(args), false);
  }

private:
  Module* ExpressionModule;
  std::vector<const FunctionSignature*> FunctionsToEmit;

  Value* generate(std::shared_ptr<TExpression> expr, IRBuilder<> builder);
  Value* GetLLVMFunction(const FunctionSignature* signature, Module* module)
  {
    return module->getOrInsertFunction(
      signature->Name,
      getLLVMType(signature));
  }
};

