LLVMPATH=/home/sd/SanitizerDector/compiler/llvm-project-llvmorg-19.1.3/build/bin
CC=""
CLANG=$LLVMPATH/clang-19
GCC=gcc
ASAN_SYMBOLIZER_PATH=$LLVMPATH/llvm-symbolizer
UBGenHome=/home/sd
MutantHome=/home/sd/SanitizerDector/juliet_dataset/C/testcases/CWE369_Divide_by_Zero_0407TMP
SDHOME=$UBGenHome/SanitizerDector
SSEQ=$SDHOME/build/sseq
CSMITH_HOME=/home/sd/SanitizerDector/csmith
CSMITH_PATH=/home/sd/SanitizerDector/juliet_dataset/C/testcasesupport #$CSMITH_HOME/output/include
OUTPUTFILE=$MutantHome/result.txt
ReduceOUTPUTFILE=$SDHOME/creducetemp/result.txt
FilterFPOUTPUTFILE=/home/sd/SanitizerDector/mutants/results/O0/result241206_FP1.txt
OptLevel="1"
reduceOptLevel="0"

