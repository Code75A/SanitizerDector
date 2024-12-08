#!/usr/bin/bash

BASEDIR=$(dirname "$0")
source $BASEDIR/config.sh --source-only

genCompileCommand() {
    TestFile=$1
    ProgramDir=$2
    sed -e "s|\${MutantHome}|$programDir|g" -e "s|\${CSMITH_PATH}|$CSMITH_PATH|g" -e "s|\${TestFile}|$TestFile|g" $SDHOME/misc/compile_commands.json
}


checkStrNLine() {
    local log=$1
    local sourceCodeFile=$2
    local checkedStr=$3

    echo "All arguments: $2, $3"


    # findLineNumberInlog
    local line1=`echo "$log" | grep "^SUMMARY" | grep -oE ":[0-9]+(:[0-9]+)?" | head -n 1 | sed -E 's/^:([0-9]+).*/\1/'`


    # findLineNumberIn source file
    local line2=`grep -n "$checkedStr" $sourceCodeFile |  head -n 1 | cut -d: -f1`

    echo $line1 
    echo $line2
    if [ $line1 -eq $line2 ]; then
        return 0
    fi
    return 1
}

runCommandCheckString() {
    local strtoCheck=$1
    local programName=$2
    local checkedStr=$3
    local checkedStr2=$4

    time=0
    while true
    do
        ((time++))
        error_message=$(ASAN_SYMBOLIZER_PATH=$ASAN_SYMBOLIZER_PATH timeout 100 ./a.out 2>&1)
        exit_status=$?
        if [ $exit_status -eq 124 ]; then
            echo "./a.out command timeout for $programName"
            echo "./a.out command timeout for $programName" >> $OUTPUTFILE
        else
            break
        fi
        if [ $time -eq 5 ]; then
            echo "./a.out command timeout 4 times for $programName, skipping"
            echo "./a.out command timeout 4 times for $programName, skipping" >> $OUTPUTFILE
            return -1
        fi
    done

    if [ "$checkedStr2" ]; then
        if echo "$error_message" | grep -q $checkedStr2; then
            echo "buggy location is executed"
        else
            echo "buggy location is missed, skipping"
            return -1
        fi
    fi

    # Check if the error message contains a specific string
    if echo "$error_message" | grep -q $strtoCheck; then
        echo $strtoCheck triggers
        echo "$error_message"
        checkStrNLine "$error_message" $programName $checkedStr
        exit_status=$?
        if [ $exit_status -ne 0 ]; then
            echo "triggered bug is not the instrumented bug."
            return 0
        else
            echo "triggered bug is the instrumented bug."
        fi
        return 1
    else
        echo $strtoCheck does not trigger
        echo "$error_message"
        return 0
    fi
}


testOneProgram() {
    programName=$1
    programName=`readlink -f $programName`
    programDir=`dirname $programName`
    cd $programDir

    #run sseq
    echo "genCompileCommand $programName $programDir > $programDir/compile_commands.json"
    genCompileCommand $programName $programDir > $programDir/compile_commands.json
    echo "running timeout 20 $SSEQ $programName div 2>1"
    res=`timeout 20 $SSEQ $programName div 2>1`
    res=`timeout 20 $SSEQ $programName div print 2>1`
    exit_status=$?
    # if sseq failed, we skip this file
    if [ $exit_status -eq 124 ]; then
        echo "sseq command timeout for $programName div print"
        echo "sseq command timeout for $programName div print" >> $OUTPUTFILE
        return -1
    fi

    # compile _print file
    printProgName=$( basename $programName .c )
    printProgName=$printProgName"_print.c"
    echo $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=undefined $printProgName
    $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=undefined $printProgName &> /dev/null
    # for creduce, avoid accept files with syntax errors.
    if [[ $? -ne 0  ]]; then
        echo "compile error!"
        return -1
    fi
    sleep 0.5

    echo "runing UndefinedBehaviorSanitizer"
    runCommandCheckString "division" $printProgName "/*UBFUZZ/*" "ACT_CHECK_CODE"
    local res1=$?
    echo "UBS finished"

    NewProgName=$( basename $programName .c )
    NewProgName=$NewProgName"_div.c"

    retval=1

    # if adding array is failed, we skip this file
    if grep -q "=SaniTestArr0" "$NewProgName"; then
        echo "SaniTestArr0 access found in $NewProgName"
    else
        echo "SaniTestArr0 access not found in $NewProgName"
        echo "SaniTestArr0 access not found in $NewProgName" >> $OUTPUTFILE
        return "$retval"

    fi

    cd $programDir
    echo $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=address $NewProgName
    $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=address $NewProgName &> /dev/null
    # for creduce, avoid accept files with syntax errors.
    if [[ $? -ne 0  ]]; then
        echo "compile error!"
        return -1
    fi
    sleep 0.5
    echo "runing AddressSanitizer"
    runCommandCheckString "AddressSanitizer" $NewProgName "/*A_QUITE_UNIQUE_FLAG/*"
    local res2=$?
    echo "AS finished, the results are $res1 vs $res2"

    if [[ $res1 -eq 255 || $res2 -eq 255 ]]; then
        echo "error occured"

    elif [ $res1 -ne $res2 ]; then
        echo "found a bug"
        echo "found a bug in $programName" >> $OUTPUTFILE
        retval=0
    else
        echo "does not find a bug"
    fi
    return "$retval"

}


ProgressBar() {
    # Process data
    let _progress=(${1}*100/${2}*100)/100
    let _done=(${_progress}*4)/10
    let _left=40-$_done
    # Build progressbar string lengths
    _fill=$(printf "%${_done}s")
    _empty=$(printf "%${_left}s")

    # 1.2 Build progressbar strings and print the ProgressBar line
    # 1.2.1 Output example:
    # 1.2.1.1 Progress : [########################################] 100%
    printf "\rProgress : [${_fill// /#}${_empty// /-}] ${_progress}%%"

}

testing() {
    echo "======starting=====" >> $OUTPUTFILE
    cd $MutantHome
    res=`find $MutantHome -name "*.c"`
    totalNum=`echo "$res" | wc -l`
    number=1
    while read filename
    do
        # we ignore files ends with _test.c, since it is an instrumented file generated by our tool.
        if [[ $filename == *_test.c  ]]; then
            continue
        fi
        testOneProgram $filename
        ProgressBar ${number} ${totalNum}
        ((number++))
    done <<< "$res"
}

if [ "${1}" != "--source-only"  ]; then
    testing
    #testOneProgram /bigdata/fff000/UBGen/mutants/mutated_2_tmpq3c8wha8.c
    #testOneProgram /bigdata/fff000/UBGen/mutants/mutated_1_tmpcniozzx3.c
fi




