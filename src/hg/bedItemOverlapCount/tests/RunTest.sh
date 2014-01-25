#!/bin/sh

TESTPROG=bedItemOverlapCount
tests=0
failures=0
verbose=""
export TESTPROG verbose tests failures

usage() {
    echo "usage: `basename $0` [-verbose] [-home] [-help]"
    echo -e "\tTesting the binary $TESTPROG\n"
    echo -e "\t-verbose - show all test command lines and results"
    echo -e "\t-home - use binary $HOME/bin/$MACHTYPE/${TESTPROG}"
    echo -e "\t\t (default uses your PATH to find the binaries)"
    echo -e "\t-help - print this usage message"
    exit 255
}

oneTest() {
    tests=`expr $tests + 1`
    cmd=$1
    expected=$2

    C=`echo $tests | awk '{printf "%4d", $1}'`
    T=`eval $cmd | grep -v "^#.output date:" | sed -e "s/, output date:.*//" | sum -r`

    if [ "${T}" != "$expected" ]; then
	echo "${C} FAIL: $cmd"
	echo "expected: '$expected' got: '$T'"
	failures=`expr $failures + 1`
    else
	if [ -n "${verbose}" ]; then
	    echo "${C}   OK: $cmd"
	fi
    fi
}

failedArgCheck=0
export failedArgCheck

while [ "$#" -gt 0 ]
do
    if [ "$1" = "-help" -o "$1" = "--help" ]; then
	usage
    elif [ "$1" = "-verbose" ]; then
	verbose="verbose"
    elif [ "$1" = "-home" ]; then
	TESTPROG="$HOME/bin/$MACHTYPE/$TESTPROG"
    else
	echo "unknown argument: $1"
	failedArgCheck=1
    fi
    shift
done

export TESTPROG verbose tests failures

if [ "$failedArgCheck" -eq 1 ]; then
    exit 255
fi

type ${TESTPROG} > /dev/null 2> /dev/null

if [ "$?" -ne 0 ]; then
    echo "ERROR: can not find test program binary:"
    echo -e "\t'$$TESTPROG'"
    exit 255
fi


#  TEST 1
oneTest "$TESTPROG hg16 plusStrand.bed.gz" "08998   212"
oneTest "$TESTPROG hg16 minusStrand.bed.gz" "62695   231"

if [ -n "${verbose}" ]; then
    C=`echo $tests | awk '{printf "%4d", $1}'`
    echo "${C}   run, $failures failed";
fi

exit $failures
