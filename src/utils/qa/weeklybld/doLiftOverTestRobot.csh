#!/bin/tcsh
# note this uses java and ant, so it will not work on beta, so run from dev instead
cd $WEEKLYBLD
if ( "$HOST" != "hgwdev" ) then
 echo "error: doLiftOverTestRobot.csh must be executed from hgwdev!"
 exit 1
endif

# run on hgwdev
set log = LiftOverTest-v${BRANCHNN}.log
cd $BUILDDIR/v${BRANCHNN}_branch/kent/java
build
nohup LiftOverTest default >& $WEEKLYBLD/logs/$log 

echo "LiftOverTest robot done. Check to see if any errors in $WEEKLYBLD/logs/$log."

exit 0

