LLVMPATH=/bigdata/fff000/UBGen/llvm_under_tested/LLVM-19.1.0-Linux-X64/bin
CC=$LLVMPATH/clang-19
ASAN_SYMBOLIZER_PATH=$LLVMPATH/llvm-symbolizer
UBGenHome=/home/sd
MutantHome=$UBGenHome/mutants
SDHOME=$UBGenHome/SanitizerDector
SSEQ=$SDHOME/build/sseq
CSMITH_HOME=/home/sd/SanitizerDector/csmith
CSMITH_PATH=$CSMITH_HOME/output/include
OUTPUTFILE=$SDHOME/result.txt


