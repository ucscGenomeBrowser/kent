#!/bin/sh
#	mkUsageIndex.sh - create index of usage messages from current
#	binaries on kolossus in /cluster/bin/x86_64

#	$Id: mkUsageIndex.sh,v 1.2 2006/04/04 19:25:02 hiram Exp $

usage() {
    echo "mkUsageIndex.sh - create index of usage messages"
    echo "usage: mkUsageIndex.sh <anything> > useMessageIndex.html"
    echo "expects to be working on kolossus"
    exit 255
}

if [ $# -lt 1 ]; then
    usage
fi

cd /cluster/bin/x86_64
find . -type f -newer randomPlacement \
	| sed -e "s/^\.\///" | sort > /tmp/x86_64.list

echo "<HTML><HEAD><TITLE>kent source tree programs</TITLE></HEAD>"
echo "<BODY>"
echo "<TABLE>"

cd /tmp
for F in `cat x86_64.list | egrep -v "windowmasker|hgCGAP"`
do
    rm -f /tmp/useMes.txt
    /cluster/bin/x86_64/${F} > /tmp/useMes.txt 2>&1
    TEXT="(execute error)"
    if [ -s /tmp/useMes.txt ]; then
    TEXT=`grep -v pslDiff /tmp/useMes.txt | head -3 | \
	sed -e "/^[Uu][Ss][Aa][Gg][Ee]:/,$ d" |
	sed -e "/^Error: wrong/d; /^wrong # of args/d; /^$/d" |
	sed -e "/^options$/d" |
	sed -e "s/^${F} - //"`
    fi
    echo "<TR><TD ALIGN=LEFT>${F}:</TD><TD>${TEXT}</TD></TR>"
done

echo "</TABLE>"
echo "</BODY></HTML>"

