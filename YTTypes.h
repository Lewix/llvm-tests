#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/LLVMContext.h"
using namespace llvm;

typedef int64_t i64;
typedef int32_t i32;
typedef int8_t i8;
typedef uint64_t ui64;
typedef uint32_t ui32;
typedef uint8_t ui8;

/* YT data types */

enum EValueType {
  Null,
  Int64,
  Uint64,
  Double,
  Boolean,
  String,
  Any // Don’t bother right now.
};

struct TValue {
  i8 Id; // Column name.
  i8 Type; // Column type (EValueType).
  i32 Length; // For string-like values this fields contains size of variable part.
  union {
    i64 Int64;
    ui64 Uint64;
    double Double;
    bool Boolean;
    const char* String;
  } Data;
};

struct TRowHeader
{
  int Count; // Number of values in a row.
  int Padding; // Pads TRowHeader to be 16-byte.
};

using TRow = TRowHeader*;

/* LLMV types for YT data types */

namespace llvm {
template<bool xcompile> class TypeBuilder<TValue, xcompile> {
 public:
  static StructType* get(LLVMContext &context) {
    return StructType::get(
      TypeBuilder<types::i<16>, xcompile>::get(context),
      TypeBuilder<types::i<16>, xcompile>::get(context),
      TypeBuilder<types::i<32>, xcompile>::get(context),
      TypeBuilder<types::i<64>, xcompile>::get(context),
      NULL);
  }
};

template<bool xcompile> class TypeBuilder<TRowHeader, xcompile> {
 public:
  static StructType* get(LLVMContext &context) {
    return StructType::get(
      TypeBuilder<types::i<32>, xcompile>::get(context),
      TypeBuilder<types::i<32>, xcompile>::get(context),
      NULL);
  }
};
}

