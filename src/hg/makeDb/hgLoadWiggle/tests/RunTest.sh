#!/bin/sh

HGWIGGLE=hgWiggle
WIGENCODE=wigEncode
HGLOADWIGGLE=hgLoadWiggle
tests=0
failures=0
verbose=""
export HGWIGGLE WIGENCODE verbose tests failures

usage() {
    echo "usage: `basename $0` [-verbose] [-home] [-help]"
    echo -e "\tTesting the binary $HGWIGGLE and $WIGENCODE\n"
    echo -e "\t-verbose - show all test command lines and results"
    echo -e "\t-home - use binary $HOME/bin/$MACHTYPE/..."
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
	HGWIGGLE="$HOME/bin/$MACHTYPE/$HGWIGGLE"
	WIGENCODE="$HOME/bin/$MACHTYPE/$WIGENCODE"
	HGLOADWIGGLE="$HOME/bin/$MACHTYPE/$HGLOADWIGGLE"
    else
	echo "unknown argument: $1"
	failedArgCheck=1
    fi
    shift
done

export HGWIGGLE WIGENCODE verbose tests failures

if [ "$failedArgCheck" -eq 1 ]; then
    exit 255
fi

type ${HGWIGGLE} > /dev/null 2> /dev/null

if [ "$?" -ne 0 ]; then
    echo "ERROR: can not find hgWiggle binary"
    echo -e "\t'$HGWIGGLE'"
    exit 255
fi

type ${WIGENCODE} > /dev/null 2> /dev/null

if [ "$?" -ne 0 ]; then
    echo "ERROR: can not find wigEncode binary"
    echo -e "\t'$WIGENCODE'"
    exit 255
fi

TF=/tmp/wigEncode.$$.txt
TF_WIG=/tmp/wigEncode.$$.wig
wget "http://genome.ucsc.edu/encode/wiggleExample.txt" -O "${TF}" \
	> /dev/null 2> /dev/null
#  TEST 1 - wigEncode testing
oneTest "${WIGENCODE} ${TF} stdout /dev/null 2> /dev/null" "04152     2"

${WIGENCODE} ${TF} ${TF_WIG} /dev/null 2> /dev/null


#  TEST 2 - hgLoadWiggle testing - can not use oneTest for this
rm -f wiggle.tab
cmd="${HGLOADWIGGLE} -noLoad hg16 testTrack ${TF_WIG}"
${HGLOADWIGGLE} -noLoad hg16 testTrack "${TF_WIG}" 2> /dev/null
T=`sum -r wiggle.tab`
expected="44905     2"
tests=`expr $tests + 1`
C=`echo $tests | awk '{printf "%4d", $1}'`

if [ "${T}" != "$expected" ]; then
    echo "${C} FAIL: $cmd"
    echo "expected: '$expected' got: '$T'"
    failures=`expr $failures + 1`
else
    if [ -n "${verbose}" ]; then
	echo "${C}   OK: $cmd"
    fi
fi
#  clean up test 1 and 2 garbage
rm -f "${TF}" "${TF_WIG}" wiggle.tab

#  TEST 3 - hgWiggle testing
oneTest "$HGWIGGLE -lift=1 -chr=chrM -db=ce2 gc5Base" "45724    31"
oneTest "$HGWIGGLE -chr=chrM -doBed -db=ce2 gc5Base" "37864     1"
oneTest "$HGWIGGLE -chr=chrM -doStats -db=ce2 gc5Base" "40185     1"
oneTest "$HGWIGGLE -lift=1 -position=chrM:4000-10000 -db=ce2 gc5Base" "52320    14"
oneTest "$HGWIGGLE -lift=1 -position=chrM:4000-10000 -dataConstraint=\">\" -ll=50 -db=ce2 gc5Base" "22598     3"
oneTest "$HGWIGGLE -lift=1 -position=chrM:4000-10000 -dataConstraint=\"<=\" -ll=50 -db=ce2 gc5Base" "58301    12"
oneTest "$HGWIGGLE -lift=1 -position=chrM:4000-10000 -dataConstraint=\"in range\" -ll=23 -ul=60 -db=ce2 gc5Base" "28698     5"
oneTest "$HGWIGGLE -doBed -position=chrM:4000-10000 -dataConstraint=\">\" -ll=50 -db=ce2 gc5Base" "15056     3"
oneTest "$HGWIGGLE -doStats -position=chrM:4000-10000 -dataConstraint=\">\" -ll=50 -db=ce2 gc5Base" "38601     1"
oneTest "$HGWIGGLE -doBed -doStats -position=chrM:4000-10000 -dataConstraint=\">\" -ll=50 -db=ce2 gc5Base" "14143     4"
oneTest "$HGWIGGLE -doBed -position=chrM:4000-10000 -dataConstraint=\"<=\" -ll=50 -db=ce2 gc5Base" "51931     3"
oneTest "$HGWIGGLE -doStats -doBed -position=chrM:4000-10000 -dataConstraint=\"<=\" -ll=50 -db=ce2 gc5Base" "42822     4"
oneTest "$HGWIGGLE -doBed -position=chrM:4000-10000 -dataConstraint=\"in range\" -ll=23 -ul=60 -db=ce2 gc5Base" "04329     6"
oneTest "$HGWIGGLE -doStats -position=chrM:4000-10000 -dataConstraint=\"in range\" -ll=23 -ul=60 -db=ce2 gc5Base" "54084     1"
oneTest "$HGWIGGLE -doStats -doBed -position=chrM:4000-10000 -dataConstraint=\"in range\" -ll=23 -ul=60 -db=ce2 gc5Base" "06015     6"
oneTest "$HGWIGGLE -span=5 -doHistogram -hBinSize=10 -hBinCount=10 -position=chrM:4000-10000 -dataConstraint=\"in range\" -ll=3 -ul=90 -db=ce2 gc5Base" "44411     1"
oneTest "$HGWIGGLE -bedFile=mm5.miRNA.bed -dataConstraint=\">\" -ll=0.85 -chr=chr2 -doStats -db=mm5 phastCons" "62191     1"

TF=/tmp/hgWiggle_test_$$.bed
rm -f "${TF}"
echo -e "chr7\t73016533\t73016543\tname0" > "${TF}"
echo -e "chr1\t2500\t2510\tname1" >> "${TF}"
echo -e "chr10\t200000\t200010\tname2" >> "${TF}"
echo -e "chr1\t1500\t1510\tname3" >> "${TF}"

oneTest "$HGWIGGLE -bedFile=${TF} -db=hg17 phastCons" "17020     2"

rm -f "${TF}"

if [ -n "${verbose}" ]; then
    C=`echo $tests | awk '{printf "%4d", $1}'`
    echo "${C}   run, $failures failed";
fi

exit 0
