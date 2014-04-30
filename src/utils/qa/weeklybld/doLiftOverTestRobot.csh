#!/bin/tcsh
# note this uses java and ant, so it will not work on beta, so run from dev instead
cd $WEEKLYBLD
if ( "$HOST" != "hgwdev" ) then
 echo "error: doLiftOverTestRobot.csh must be executed from hgwdev!"
 exit 1
endif

# run on hgwdev
set log = LiftOverTest-v${BRANCHNN}.log
cd $JAVABUILD
nohup LiftOverTest kent/java/src/edu/ucsc/genome/util/LiftOverTest.props >& $WEEKLYBLD/logs/$log 

echo "LiftOverTest robot done. Check to see if any errors in $WEEKLYBLD/logs/$log."
echo "LiftOverTest robot done. Check to see if any errors in $WEEKLYBLD/logs/$log." \
  | mail -s "v${BRANCHNN} LiftOverTest robot done." $USER ${BUILDMEISTER} browser-qa

exit 0
