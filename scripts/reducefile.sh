#!/bin/bash
programName=$1
creducetempDir="creducetemp"
mkdir $creducetempDir
cp $programName $creducetempDir/reduce.c

creduce creduce.sh $creducetempDir/reduce.c



