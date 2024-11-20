#!/bin/bash
BASEDIR=$(dirname "$0")
programName=$(readlink -f $1)
creducetempDir="creducetemp"
mkdir $creducetempDir
creducetempDir=$(readlink -f $creducetempDir)
#reduceFilePath=$creducetempDir/reduce.c
reduceFilePath=./reduce.c
echo "the reduced file path is $reduceFilePath, orig file is created in the same dir."
cp $programName $reduceFilePath

echo creduce $BASEDIR/creduce.sh $creducetempDir/reduce.c
creduce $BASEDIR/creduce.sh $reduceFilePath



