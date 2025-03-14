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
    #when there is no SUMMARY
    #if [ -z "$line1" ]; then
    #echo aaa: $checkedStr
    if [[ "$checkedStr" == "/*UBFUZZ/*" ]]; then
        line1=$(echo "$log" | grep "runtime error: division by zero" | grep -oE ":[0-9]+(:[0-9]+)?" | head -n 1 | sed -E 's/^:([0-9]+).*/\1/')
    fi


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
            return -1
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
    CC=$CLANG
    testOneProgramWithOneCompiler $1
    clangflag=$?
    clangres1=$res1
    clangres2=$res2

    CC=$GCC
    testOneProgramWithOneCompiler $1
    gccflag=$?
    gccres1=$res1
    gccres2=$res2

    if [[ $clangflag -eq 0 || $gccflag -eq 0 ]]; then
        echo "found a bug in $programName, clang: $clangres1 vs $clangres2, gcc: $gccres1 vs $gccres2 " >> $OUTPUTFILE
    else
        return 1
    fi

    #modify this for reduce
    if [[ $clangres1 -ne 0 || $clangres2 -ne 1 ]]; then
        return 1
    fi
    if [[ $gccres1 -ne 0 || $gccres2 -ne 1 ]]; then
        return 1
    fi
    return 0
}

testOneProgramWithOneCompiler() {
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
    echo $CC -g -O$OptLevel -Wno-everything -I$CSMITH_PATH -fsanitize=undefined $printProgName
    $CC -g -O$OptLevel -Wno-everything -I$CSMITH_PATH -fsanitize=undefined $printProgName &> /dev/null
    # for creduce, avoid accept files with syntax errors.
    if [[ $? -ne 0  ]]; then
        echo "compile error!"
        return -1
    fi
    sleep 0.5

    echo "runing UndefinedBehaviorSanitizer"
    runCommandCheckString "division" $printProgName "/*UBFUZZ/*" "ACT_CHECK_CODE"
    res1=$?
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
    echo $CC -g -O$OptLevel -Wno-everything -I$CSMITH_PATH -fsanitize=address $NewProgName
    $CC -g -O$OptLevel -Wno-everything -I$CSMITH_PATH -fsanitize=address $NewProgName &> /dev/null
    # for creduce, avoid accept files with syntax errors.
    if [[ $? -ne 0  ]]; then
        echo "compile error!"
        return -1
    fi
    sleep 0.5
    echo "runing AddressSanitizer"
    runCommandCheckString "AddressSanitizer" $NewProgName "/*A_QUITE_UNIQUE_FLAG/*"
    res2=$?
    echo "AS finished, the results are $res1 vs $res2"

    if [[ $res1 -eq 255 || $res2 -eq 255 ]]; then
        echo "error occured"

    elif [ $res1 -ne $res2 ]; then
        echo "found a bug of $CC"
        #echo "found a bug in $programName of $CC, $res1 vs $res2 " >> $OUTPUTFILE
        retval=0
    else
        echo "does not find a bug"
        #echo "$programName of $CC, $res1 vs $res2 " >> $OUTPUTFILE
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
    printf "\rProgress : [${_fill// /#}${_empty// /-}] ${_progress}%%\n"

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
            ((number++))
            continue
        fi
        if [[ $filename == *_div.c  ]]; then
            ((number++))
            continue
        fi
        if [[ $filename == *_print.c  ]]; then
            ((number++))
            continue
        fi
        if [[ $filename == *_fp1.c  ]]; then
            ((number++))
            continue
        fi

        # 获取当前文件的目录和文件名
        dir=$(dirname "$filename")
        base=$(basename "$filename" .c)
        # 构造对应的 _div.c 文件名
        div_file="${dir}/${base}_div.c"
        # 检查 _div.c 文件是否存在
        if [ -f "$div_file" ]; then
            echo "File $div_file exists for $filename"
            ((number++))
            continue
        fi

        testOneProgram $filename
        ProgressBar ${number} ${totalNum}
        ((number++))
    done <<< "$res"
}

init() {
    cd $MutantHome
    res=`find $MutantHome -name "*.c"`
    while read filename
    do
        if [[ $filename == *_test.c  ]]; then
            rm $filename
        fi
        if [[ $filename == *_div.c  ]]; then
            rm $filename
        fi
        if [[ $filename == *_print.c  ]]; then
            rm $filename
        fi
        if [[ $filename == *_fp1.c  ]]; then
            rm $filename
        fi

    done <<< "$res"
}

if [ "${1}" != "--source-only"  ]; then
    #init
    #testing
    #testOneProgram /home/sd/SanitizerDector/mutants/results/mutantsfiles/a.c
    #testOneProgram /home/sd/SanitizerDector/mutants/mutants241216/mutated_1_tmpkro_pbdo.c
    testOneProgram /home/sd/SanitizerDector/juliet_dataset/C/testcases/CWE369_Divide_by_Zero/s01/CWE369_Divide_by_Zero__float_connect_socket_01.c
fi




