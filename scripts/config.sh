LLVMPATH=/home/sd/SanitizerDector/compiler/llvm-project-llvmorg-19.1.3/build/bin
CC=""
CLANG=$LLVMPATH/clang-19
GCC=gcc
ASAN_SYMBOLIZER_PATH=$LLVMPATH/llvm-symbolizer
UBGenHome=/home/sd
MutantHome=/home/sd/SanitizerDector/mutants/mutants241214
SDHOME=$UBGenHome/SanitizerDector
SSEQ=$SDHOME/build/sseq
CSMITH_HOME=/home/sd/SanitizerDector/csmith
CSMITH_PATH=$CSMITH_HOME/output/include
OUTPUTFILE=$MutantHome/result.txt
ReduceOUTPUTFILE=$SDHOME/creducetemp/result.txt