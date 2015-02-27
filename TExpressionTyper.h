#include "YTTypes.h"
#include "TExpression.h"
#include "FunctionRegistry.h"

namespace TExpressionTyper {
// Returns the type of TExpression expr using the signatures in the
// FunctionRegistry. If the expression does not conform to those signatures,
// EValueType::Null is returned.
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
}
