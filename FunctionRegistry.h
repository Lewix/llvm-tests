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
  // When called, IREmitter creates a Module containing this function's
  // definition and returns it
  std::function<Module*(IRBuilder<>&)> IREmitter;
};

// Registry containing metadata about functions and operators.
// Overloaded functions or operators can be retrieved by specifying
// restrictions in the resultType and argTypes arguments of GetFunction
class FunctionRegistry {
public:
  void AddFunction(const FunctionSignature& function);

  const FunctionSignature* GetFunction(
    const std::string& functionName,
    const EValueType resultType = EValueType::Null,
    const std::vector<EValueType>* argTypes = NULL);

  const FunctionSignature* GetFunction(
    const EBinaryOp opcode,
    const EValueType resultType = EValueType::Null,
    const std::vector<EValueType>* argTypes = NULL);

  // Returns the name of the registry entry for a given opcode
  static std::string getOperatorName(const EBinaryOp opcode);

private:
  std::unordered_map<std::string, std::vector<FunctionSignature>> FunctionMap;
};

void FunctionRegistry::AddFunction(const FunctionSignature& function)
{
  if (!FunctionMap.count(function.Name)) {
    std::vector<FunctionSignature> empty;
    FunctionMap.insert(std::make_pair(function.Name, empty));
  }
  std::vector<FunctionSignature>& overloads = FunctionMap.at(function.Name);
  overloads.push_back(function);
}

const FunctionSignature* FunctionRegistry::GetFunction(
  const std::string& functionName,
  const EValueType resultType,
  const std::vector<EValueType>* argTypes)
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

const FunctionSignature* FunctionRegistry::GetFunction(
  const EBinaryOp opcode,
  const EValueType resultType,
  const std::vector<EValueType>* argTypes)
{
  return GetFunction(getOperatorName(opcode), resultType, argTypes);
}

std::string FunctionRegistry::getOperatorName(const EBinaryOp opcode)
{
  switch (opcode) {
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


FunctionRegistry* registry = new FunctionRegistry();
