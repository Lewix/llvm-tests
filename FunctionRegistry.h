#include <unordered_map>
#include <memory>
#include "llvm/IR/IRBuilder.h"

struct FunctionSignature {
  FunctionSignature(
    std::string name,
    std::vector<EValueType> argumentTypes,
    EValueType returnType,
    std::function<Module*(IRBuilder<>&)> irEmitter)
    : Name(name)
    , ArgumentTypes(argumentTypes)
    , ReturnType(returnType)
    , IREmitter(irEmitter)
  { }

  FunctionSignature(
    const FunctionSignature& other)
    : Name(other.Name)
    , ArgumentTypes(other.ArgumentTypes)
    , ReturnType(other.ReturnType)
    , IREmitter(other.IREmitter)
   { }

  std::string Name;
  std::vector<EValueType> ArgumentTypes;
  EValueType ReturnType;
  std::function<Module*(IRBuilder<>&)> IREmitter;
};

class FunctionRegistry {
public:
  void AddFunction(const FunctionSignature& function)
  {
    if (!FunctionMap.count(function.Name)) {
      std::vector<FunctionSignature> empty;
      FunctionMap.insert(std::make_pair(function.Name, empty));
    }
    std::vector<FunctionSignature>& overloads = FunctionMap.at(function.Name);
    overloads.push_back(function);
  }

  const FunctionSignature* GetFunction(
    const std::string& functionName,
    const EValueType resultType = EValueType::Null,
    const std::vector<EValueType>* argTypes = NULL)
  {
    std::vector<FunctionSignature> overloads = FunctionMap.at(functionName);

    int i = 0;
    for (auto signature = overloads.begin();
         signature != overloads.end();
         signature++, i++) {
      if (resultType != EValueType::Null
          && resultType != signature->ReturnType) {
        continue;
      }

      if (argTypes != NULL
          && *argTypes != signature->ArgumentTypes) {
        continue;
      }
      return &FunctionMap.at(functionName)[i];
    }
    return NULL;
  }

  const FunctionSignature* GetFunction(
    const EBinaryOp opCode,
    const EValueType resultType = EValueType::Null,
    const std::vector<EValueType>* argTypes = NULL)
  {
    return GetFunction(getOperatorName(opCode), resultType, argTypes);
  }

  static std::string getOperatorName(const EBinaryOp opCode)
  {
    switch (opCode) {
      case Plus:
        return "+";
      case Minus:
        return "-";
      case Multiply:
        return "*";
      case Divide:
        return "/";
      case Modulo:
        return "%";
      case And:
        return "&&";
      case Or:
        return "||";
      case Equal:
        return "==";
      case NotEqual:
        return "!=";
      case Less:
        return "<";
      case LessOrEqual:
        return "<=";
      case Greater:
        return ">";
      case GreaterOrEqual:
        return ">=";
    }
  }

private:
  std::unordered_map<std::string, std::vector<FunctionSignature>> FunctionMap;
};

FunctionRegistry* registry = new FunctionRegistry();
