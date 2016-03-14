#!/bin/tcsh

cd $WEEKLYBLD

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

echo "Moving preview2 tag to current tip versions on $HOST [${0}: `date`]"

@ NEXTNN=$BRANCHNN + 1

# tag the branch preview2
#  which marks the point for git reports
#  this should not fail typically so -f to force not needed
git push origin origin:refs/tags/v${NEXTNN}_preview2${preview2Subversion}
if ( $status ) then
 echo "git shared-repo tag failed for tag v${NEXTNN}_preview2 on $HOST [${0}: `date`]"
 exit 1
endif
echo "new preview2 tag for future branch v${NEXTNN} created on $HOST. [${0}: `date`]"
git fetch   # required

