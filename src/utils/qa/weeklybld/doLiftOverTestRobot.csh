#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwdev" ) then
 echo "error: doLiftOverTestRobot.csh must be executed from hgwdev!"
 exit 1
endif

set log = v${BRANCHNN}.LiftOverTest.log
cd $JAVABUILD
nohup LiftOverTest kent/java/src/edu/ucsc/genome/util/LiftOverTest.props >& $WEEKLYBLD/logs/$log 

echo "LiftOverTest robot done. Check to see if any errors in $WEEKLYBLD/logs/$log."
echo "LiftOverTest robot done. Check to see if any errors in $WEEKLYBLD/logs/$log." \
  | mail -s "v${BRANCHNN} LiftOverTest robot done." ${BUILDMEISTEREMAIL} browser-qa@soe.ucsc.edu

exit 0
