#!/bin/tcsh
if ( "$HOST" != "hgwdev" ) then
 echo "Error: this script must be run from hgwdev."
 exit 1
endif

cd $WEEKLYBLD

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

echo "Moving review tag to current tip versions"

# rtag the review tag
echo
echo "moving tag review..."
cvs -d hgwdev:$CVSROOT rtag -a -F -rHEAD review kent >& /dev/null
if ( $status ) then
 echo "cvs rtag failed for tag review -rHEAD"
 exit 1
endif
echo "review tag moved to tip."

exit 0

