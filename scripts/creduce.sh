#!/bin/bash
BASEDIR=$(dirname "$0")
source $BASEDIR/config.sh --source-only
source $BASEDIR/testing.sh --source-only
source $BASEDIR/filter_fp.sh --source-only

OUTPUTFILE=$ReduceOUTPUTFILE
OptLevel=$reduceOptLevel

#programName=$1
programName=reduce.c
testOneProgram $programName
retval=$?

echo "fp1check for $programName"
fp1CheckOneProgram $programName
#if [[ "$output" =~ output:-1end ]]; then
if [[ "$output" =~ ^[^o]*output:-1end ]]; then
    echo "output -1"
else
    echo "not interested"
    exit 1
fi

if [ "$retval" == 0  ]
then
    echo "interested"
    exit 0
else
    echo "not interested"
    exit 1
fi
