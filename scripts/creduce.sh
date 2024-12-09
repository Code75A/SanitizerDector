#!/bin/bash
BASEDIR=$(dirname "$0")
source $BASEDIR/config.sh --source-only
source $BASEDIR/testing.sh --source-only

OUTPUTFILE=$ReduceOUTPUTFILE

#programName=$1
programName=reduce.c
testOneProgram $programName
retval=$?
if [ "$retval" == 0  ]
then
    echo "interested"
    exit 0
else
    echo "not interested"
    exit 1
fi
