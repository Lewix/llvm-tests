LLVMCONFIG= /usr/local/opt/llvm/bin/llvm-config

all: llvm-experiments.out  myudf.so

myudf.so: myudf
	/usr/local/opt/llvm/bin/llc myudf
	sed '/.macosx_version_min/ d' myudf.s > myudf2.s
	clang -shared myudf2.s -o myudf.so
	rm myudf{2,}.s

llvm-experiments.out: llvm-experiments.cpp YTTypes.h
	#clang++ -g -fno-rtti `$($LLVMCONFIG) --cxxflags --ldflags --system-libs --libs all` llvm-experiments.cpp -o llvm-experiments.out
	clang++ -g -fno-rtti -rdynamic -I/usr/local/Cellar/llvm/3.5.0_2/include  -D_DEBUG -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -O3  -std=c++11 -fvisibility-inlines-hidden -fno-exceptions -fno-rtti -fno-common -Woverloaded-virtual -Wcast-qual -L/usr/local/Cellar/llvm/3.5.0_2/lib -lLLVMLTO -lLLVMObjCARCOpts -lLLVMLinker -lLLVMipo -lLLVMVectorize -lLLVMBitWriter -lLLVMIRReader -lLLVMAsmParser -lLLVMTableGen -lLLVMDebugInfo -lLLVMOption -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMSelectionDAG -lLLVMAsmPrinter -lLLVMX86Desc -lLLVMX86Info -lLLVMX86AsmPrinter -lLLVMX86Utils -lLLVMJIT -lLLVMLineEditor -lLLVMMCAnalysis -lLLVMMCDisassembler -lLLVMInstrumentation -lLLVMInterpreter -lLLVMCodeGen -lLLVMScalarOpts -lLLVMInstCombine -lLLVMTransformUtils -lLLVMipa -lLLVMAnalysis -lLLVMProfileData -lLLVMMCJIT -lLLVMTarget -lLLVMRuntimeDyld -lLLVMObject -lLLVMMCParser -lLLVMBitReader -lLLVMExecutionEngine -lLLVMMC -lLLVMCore -lLLVMSupport -lz -lpthread -ledit -lcurses -lm llvm-experiments.cpp -o llvm-experiments.out
