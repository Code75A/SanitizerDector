#!/usr/bin/bash

BASEDIR=$(dirname "$0")
source $BASEDIR/config.sh --source-only

genCompileCommand() {
    TestFile=$1
    sed -e "s|\${MutantHome}|$MutantHome|g" -e "s|\${CSMITH_PATH}|$CSMITH_PATH|g" -e "s|\${TestFile}|$TestFile|g" $SDHOME/misc/compile_commands.json
}

runCommandCheckString() {
    strtoCheck=$1
    programName=$2

    time=0
    while true
    do
        ((time++))
        error_message=$(timeout 100 ./a.out 2>&1)
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
    # Check if the error message contains a specific string
    if echo "$error_message" | grep -q $strtoCheck; then
        echo $strtoCheck triggers
        echo "$error_message"
        return 1
    else
        echo $strtoCheck does not trigger
        echo "$error_message"
        return 0
    fi
}

testOneProgram() {
    programName=$1
    cd $MutantHome
    echo $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=undefined $programName
    $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=undefined $programName &> /dev/null
    sleep 0.5

    echo "runing UndefinedBehaviorSanitizer"
    runCommandCheckString "UndefinedBehaviorSanitizer" $programName
    res1=$?
    echo "UBS finished"

    #run sseq
    genCompileCommand $programName > $MutantHome/compile_commands.json
    res=`timeout 20 $SSEQ $programName 2>1`
    exit_status=$?
    # if sseq failed, we skip this file
    if [ $exit_status -eq 124 ]; then
        echo "sseq command timeout for $programName"
        echo "sseq command timeout for $programName" >> $OUTPUTFILE
        return
    fi

    NewProgName=$( basename $programName .c )
    NewProgName=$NewProgName"_test.c"

    retval=1

    # if adding array is failed, we skip this file
    if grep -q "=SaniTestArr0" "$NewProgName"; then
        echo "SaniTestArr0 access found in $NewProgName"
    else
        echo "SaniTestArr0 access not found in $NewProgName"
        echo "SaniTestArr0 access not found in $NewProgName" >> $OUTPUTFILE
        return "$retval"

    fi

    cd $MutantHome
    echo $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=address $NewProgName
    $CC -g -O0 -Wno-everything -I$CSMITH_PATH -fsanitize=address $NewProgName &> /dev/null
    sleep 0.5
    echo "runing AddressSanitizer"
    runCommandCheckString "AddressSanitizer" $NewProgName
    res2=$?
    echo "AS finished"

    if [[ $res1 -eq -1 || $res2 -eq -1  ]]; then
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
        testOneProgram $filename
        ProgressBar ${number} ${totalNum}
        ((number++))
    done <<< "$res"
}

if [ "${1}" != "--source-only"  ]; then
    testing
    #testOneProgram /bigdata/fff000/UBGen/mutants/mutated_0_tmpgig8icnj_test.c
fi




