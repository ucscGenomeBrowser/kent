#!/bin/sh

HGWIGGLE=hgWiggle
tests=0
failures=0
verbose=""

oneTest() {
    tests=`expr $tests + 1`
    cmd=$1
    result=$2

    C=`echo $tests | awk '{printf "%4d", $1}'`
    T=`eval $cmd | grep -v "^#.output date:" | sed -e "s/, output date:.*//" | sum -r`

    if [ "${T}" != "$result" ]; then
	echo "${C} FAIL: $cmd"
	failures=`expr $failures + 1`
    else
	if [ -n "${verbose}" ]; then
	    echo "${C}   OK: $cmd"
	fi
    fi
}

if [ "$1" = "-verbose" ]; then
    verbose="verbose"
fi

type $HGWIGGLE > /dev/null 2> /dev/null

if [ "$?" -ne 0 ]; then
    echo "ERROR: can not find hgWiggle binary"
    echo -e "\t'$HGWIGGLE'"
    exit 255
fi

oneTest "$HGWIGGLE -chr=chrM -db=ce2 gc5Base" "26963    31"
oneTest "$HGWIGGLE -chr=chrM -noAscii -doBed -db=ce2 gc5Base" "37864     1"
oneTest "$HGWIGGLE -chr=chrM -noAscii -doStats -db=ce2 gc5Base" "40185     1"
oneTest "$HGWIGGLE -position=chrM:4000-10000 -db=ce2 gc5Base" "15554    14"
oneTest "$HGWIGGLE -position=chrM:4000-10000 -dataConstraint=\">\" -ll=50 -db=ce2 gc5Base" "22894     2"
oneTest "$HGWIGGLE -position=chrM:4000-10000 -dataConstraint=\"<=\" -ll=50 -db=ce2 gc5Base" "01708    12"
oneTest "$HGWIGGLE -position=chrM:4000-10000 -dataConstraint=\"in range\" -ll=23 -ul=60 -db=ce2 gc5Base" "40914     5"
oneTest "$HGWIGGLE -noAscii -doBed -position=chrM:4000-10000 -dataConstraint=\">\" -ll=50 -db=ce2 gc5Base" "15056     3"
oneTest "$HGWIGGLE -noAscii -doStats -position=chrM:4000-10000 -dataConstraint=\">\" -ll=50 -db=ce2 gc5Base" "38601     1"
oneTest "$HGWIGGLE -doBed -doStats -position=chrM:4000-10000 -dataConstraint=\">\" -ll=50 -db=ce2 gc5Base" "48573     5"
oneTest "$HGWIGGLE -noAscii -doBed -position=chrM:4000-10000 -dataConstraint=\"<=\" -ll=50 -db=ce2 gc5Base" "51931     3"
oneTest "$HGWIGGLE -doStats -doBed -position=chrM:4000-10000 -dataConstraint=\"<=\" -ll=50 -db=ce2 gc5Base" "16987    15"
oneTest "$HGWIGGLE -noAscii -doBed -position=chrM:4000-10000 -dataConstraint=\"in range\" -ll=23 -ul=60 -db=ce2 gc5Base" "04329     6"
oneTest "$HGWIGGLE -noAscii -doStats -position=chrM:4000-10000 -dataConstraint=\"in range\" -ll=23 -ul=60 -db=ce2 gc5Base" "54084     1"
oneTest "$HGWIGGLE -doStats -doBed -position=chrM:4000-10000 -dataConstraint=\"in range\" -ll=23 -ul=60 -db=ce2 gc5Base" "11645    11"

if [ -n "${verbose}" ]; then
    C=`echo $tests | awk '{printf "%4d", $1}'`
    echo "${C}   run, $failures failed";
fi

exit 0
