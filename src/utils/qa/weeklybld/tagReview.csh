#!/bin/tcsh
if ( "$HOST" != "hgwbeta" ) then
 echo "Error: this script must be run from hgwbeta."
 exit 1
endif

cd $WEEKLYBLD

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

echo "Moving review tag to current tip versions [`date`]"

# rtag the review tag
echo
echo "moving tag review..."
# new way should be faster:
cvs -d hgwdev:$CVSROOT rtag -Fa review kent >& /dev/null
# note: if this breaks the cvs-reports just return it to the old way.
# old way:
#cvs -d hgwdev:$CVSROOT rtag -da review kent >& /dev/null
#cvs -d hgwdev:$CVSROOT rtag review kent >& /dev/null
if ( $status ) then
 echo "cvs rtag failed for review tag move [`date`]"
 exit 1
endif
echo "review tag moved to tip. [`date`]"


