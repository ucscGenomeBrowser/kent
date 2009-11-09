#!/bin/sh
#	mkUsageIndex.sh - create index of usage messages from current
#	binaries in /cluster/bin/x86_64

#	$Id: mkUsageIndex.sh,v 1.4 2009/11/09 21:17:13 hiram Exp $

usage() {
    echo "mkUsageIndex.sh - create index of usage messages"
    echo "usage: mkUsageIndex.sh <anything> > useMessageIndex.html"
    exit 255
}

if [ $# -lt 1 ]; then
    usage
fi

cd $HOME/bin/$MACHTYPE
find . -type f \
	| sed -e "s/^\.\///" | sort > /tmp/x86_64.list

echo "<HTML><HEAD><TITLE>kent source tree programs</TITLE></HEAD>"
echo "<BODY>"
echo "<TABLE>"

cd /tmp
for F in `cat /tmp/x86_64.list | egrep -v "aarToAxt|countNib|dumpNib|exonerateGffDoctor|cvsFiles|cvsStatusFilter|cvsup|cvscheck|hgCGAP"`
do
    rm -f /tmp/useMes.txt
    echo "$HOME/bin/$MACHTYPE/${F}" 1>&2
    $HOME/bin/$MACHTYPE/${F} > /tmp/useMes.txt 2>&1
    TEXT="(execute error)"
    if [ -s /tmp/useMes.txt ]; then
    TEXT=`grep -v pslDiff /tmp/useMes.txt | head -3 | \
	sed -e "/^[Uu][Ss][Aa][Gg][Ee]:/,$ d" |
	sed -e "/^Error: wrong/d; /^wrong # of args/d; /^$/d" |
	sed -e "/^options$/d" |
	sed -e "s/^  *//" |
	sed -e "s/^${F} - //"`
    fi
    if [ "x${TEXT}y" = "xy" -o "${TEXT}" = "(execute error)" ]; then
	echo "empty message: $F" 1>&2
    else
	echo "<TR><TD VALIGN=TOP ALIGN=LEFT>${F}:</TD><TD>${TEXT}</TD></TR>"
    fi
done

echo "</TABLE>"
echo "</BODY></HTML>"

