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
    FunctionMap.insert(std::make_pair(function.Name, function));
  }

  const FunctionSignature& GetFunction(const std::string& functionName)
  {
    return FunctionMap.at(functionName);
  }

private:
  std::unordered_map<std::string, FunctionSignature> FunctionMap;
};

FunctionRegistry* registry = new FunctionRegistry();
