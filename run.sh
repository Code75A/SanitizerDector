#!/bin/sh

CC=/bigdata/fff000/UBGen/llvm_under_tested/LLVM-19.1.0-Linux-X64/bin/clang-19
MutantHome=/bigdata/fff000/UBGen/mutants
SDHOME=/bigdata/fff000/UBGen/SanitizerDector/
SSEQ=$SDHOME/build/sseq
CSMITH_PATH=/home/fff000/csmith/include
OUTPUTFILE=$SDHOME/result.txt

genCompileCommand() {
    TestFile=$1
    sed -e "s|\${MutantHome}|$MutantHome|g" -e "s|\${CSMITH_PATH}|$CSMITH_PATH|g" -e "s|\${TestFile}|$TestFile|g" $SDHOME/misc/compile_commands.json
}

runCommandCheckString() {
    strtoCheck=$1

    error_message=$(./a.out 2>&1)
    # Check if the error message contains a specific string
    if echo "$error_message" | grep -q $strtoCheck; then
        return 1
    else
        return 0
    fi
}

testOneProgram() {
    programName=$1
    cd $MutantHome
    echo $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=undefined $programName
    $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=undefined $programName &> /dev/null
    sleep 0.5

    runCommandCheckString "UndefinedBehaviorSanitizer"
    res1=$?
    echo "UBS finished"

    #run sseq
    genCompileCommand $programName > $MutantHome/compile_commands.json
    res=`timeout 10 $SSEQ $programName 2>1`
    exit_status=$?
    if [ $exit_status -eq 124 ]; then
        echo "sseq command timeout" >> $OUTPUTFILE
        return
    fi

    NewProgName=$( basename $programName .c )
    NewProgName=$NewProgName"_test.c"
    cd $MutantHome
    echo $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=address $NewProgName
    $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=address $NewProgName &> /dev/null
    sleep 0.5
    runCommandCheckString "AddressSanitizer"
    echo "AS finished"
    res2=$?

    if [ $res1 -ne $res2 ]; then
        echo "found a bug"
        echo $programName >> $OUTPUTFILE
    else
        echo "does not find a bug"
    fi

}

testing() {
    echo "======starting=====" >> $OUTPUTFILE
    cd $MutantHome
    for filename in $MutantHome/*.c; do
        #echo $filename
        testOneProgram $filename
    done
}

testing
#testOneProgram /bigdata/fff000/UBGen/mutants/mutated_0_tmpi8sgpa2c.c




