#!/bin/bash
BASEDIR=$(dirname "$0")
source $BASEDIR/config.sh --source-only
export CSMITH_HOME=$CSMITH_HOME
#temp=$CSMITH_HOME
#export CSMITH_HOME=$temp

function ProgressBar {
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


count=100

echo "starting"
for number in $(seq $count)
do
    $UBGenHome/ubgen.py --ub division-by-zero --out $MutantHome --csmith $CSMITH_HOME
    ProgressBar ${number} ${count}
done

$BASEDIR/testing.sh
