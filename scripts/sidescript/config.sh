LLVMPATH=/root/sseq/support/llvm-project-llvmorg-19.1.3/build/bin
CC=""
CLANG=$LLVMPATH/clang-19
GCC=gcc
ASAN_SYMBOLIZER_PATH=$LLVMPATH/llvm-symbolizer
UBGenHome=/root


MutantHome=/root/sourcecode_tobuild/libzip-main


SDHOME=$UBGenHome/sseq
SSEQ=$SDHOME/build/sseq
CSMITH_HOME=/root/sseq/support/csmith


CSMITH_PATH=""


OUTPUTFILE=$MutantHome/result.txt