#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwdev" ) then
 echo "error: doTrackCheckRobot.csh must be executed from hgwdev!"
 exit 1
endif

# run on hgwdev
set log = TrackCheck-v${BRANCHNN}.log
cd $BUILDDIR/v${BRANCHNN}_branch/kent/java
build
cd src/edu/ucsc/genome/qa/cgiCheck
nohup TrackCheck full.props >& $WEEKLYBLD/logs/$log 
# tail -f $WEELYBLD/logs/$log
# egrep -y "not|warn|err" $WEEKLYBLD/logs/$log

echo "TrackCheck robot done. Check to see if any errors in $WEEKLYBLD/logs/$log."

exit 0

