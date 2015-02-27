#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"

class LLVMCodegen {
public:
  LLVMCodegen() : ExpressionModule(new Module("expr", getGlobalContext())) { }

  Module* GetExpressionModule(std::shared_ptr<TExpression> expr)
  {
    LLVMContext& context = getGlobalContext();
    IRBuilder<> builder(context);
    FunctionType* funTp = TypeBuilder<types::i<64>(void), true>::get(context);
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
    Type* result = getLLVMType(signature->ReturnType);
    std::vector<Type*> args;
    auto argEValueTp = signature->ArgumentTypes.begin();
    for (; argEValueTp != signature->ArgumentTypes.end(); argEValueTp++) {
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

