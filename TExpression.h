
using namespace llvm;

enum EBinaryOp {
    // Arithmetical operations. (int, uint, double)
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
