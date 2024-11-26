LLVMPATH=/home/sd/SanitizerDector/compiler/llvm-project-llvmorg-19.1.3/build/bin
CC=$LLVMPATH/clang-19
ASAN_SYMBOLIZER_PATH=$LLVMPATH/llvm-symbolizer
UBGenHome=/home/sd
MutantHome=$UBGenHome/mutants
SDHOME=$UBGenHome/SanitizerDector
SSEQ=$SDHOME/build/sseq
CSMITH_HOME=/home/sd/SanitizerDector/csmith
CSMITH_PATH=$CSMITH_HOME/output/include
OUTPUTFILE=$SDHOME/result.txt


