#include <iostream>
#include <utility>
#include <string>
#include <functional>
#include <unordered_map>
#include "YTTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/LLVMContext.h"
using namespace llvm;

enum EBinaryOp {
    // Arithmetical operations.
    Plus,
    Minus,
    Multiply,
    Divide,
    // Integral operations.
    Modulo,
    // Logical operations.
    And,
    Or,
    // Relational operations.
    Equal,
    NotEqual,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual
};

struct TExpression {
  TExpression(EValueType type) : Type(type)
  { }

  virtual ~TExpression() { }

  const EValueType Type;

  std::string GetName() const;

  template <class TDerived>
  const TDerived* As() const
  {
      return dynamic_cast<const TDerived*>(this);
  }

  template <class TDerived>
  TDerived* As()
  {
      return dynamic_cast<TDerived*>(this);
  }
};

struct TLiteralExpression
    : public TExpression
{
    TLiteralExpression(EValueType type) : TExpression(type)
    { }

    virtual ~TLiteralExpression() { }

    TLiteralExpression(
        EValueType type,
        std::shared_ptr<TValue> value)
        : TExpression(type)
        , Value(value)
    { }

    std::shared_ptr<TValue> Value;
};

typedef std::shared_ptr<TExpression> TConstExpressionPtr;
typedef std::vector<TConstExpressionPtr> TArguments;

struct TFunctionExpression
    : public TExpression
{
    TFunctionExpression(EValueType type) : TExpression(type)
    { }

    TFunctionExpression(
        EValueType type,
        const std::string& functionName,
        const TArguments& arguments)
        : TExpression(type)
        , FunctionName(functionName)
        , Arguments(arguments)
    { }

    std::string FunctionName;
    TArguments Arguments;
};

struct TBinaryOpExpression
    : public TExpression
{
    TBinaryOpExpression(EValueType type) : TExpression(type)
    { }

    TBinaryOpExpression(
        EValueType type,
        EBinaryOp opcode,
        const TConstExpressionPtr& lhs,
        const TConstExpressionPtr& rhs)
        : TExpression(type)
        , Opcode(opcode)
        , Lhs(lhs)
        , Rhs(rhs)
    { }

    EBinaryOp Opcode;
    TConstExpressionPtr Lhs;
    TConstExpressionPtr Rhs;
};

struct FunctionSignature {
  FunctionSignature(
    std::string name,
    std::vector<EValueType> argumentTypes,
    EValueType returnType,
    std::function<void(IRBuilder<>&)> irEmitter)
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
  std::function<void(IRBuilder<>&)> IREmitter;

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

FunctionRegistry* registry;

void emitPlus(IRBuilder<>& builder)
{
  //TODO
}

EValueType typeOf(const TExpression* expr)
{
  if (expr->As<TLiteralExpression>()) {
    return expr->Type;

  } else if (expr->As<TBinaryOpExpression>()) {
    const TBinaryOpExpression* binOpExpr = expr->As<TBinaryOpExpression>();
    EValueType lhsType = typeOf(binOpExpr->Lhs.get());
    EValueType rhsType = typeOf(binOpExpr->Rhs.get());
    if (lhsType == rhsType) {
      return lhsType;
    }

  } else if (expr->As<TFunctionExpression>()) {
    const TFunctionExpression* funExpr = expr->As<TFunctionExpression>();
    const FunctionSignature& signature =
      registry->GetFunction(funExpr->FunctionName);
    auto arg = funExpr->Arguments.begin();
    auto tpe = signature.ArgumentTypes.begin();
    for (; arg != funExpr->Arguments.end();
         arg++, tpe++) {
      if (typeOf(arg->get()) != *tpe) {
        return EValueType::Null;
      }
    }
    return signature.ReturnType;
  }

  return EValueType::Null;
}

int main(int argc, char** argv)
{
  registry = new FunctionRegistry();
  std::vector<EValueType> plusArgTypes;
  plusArgTypes.push_back(EValueType::Int64);
  plusArgTypes.push_back(EValueType::Int64);
  registry->AddFunction(FunctionSignature(
    "+", plusArgTypes, EValueType::Int64, emitPlus));

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
}
