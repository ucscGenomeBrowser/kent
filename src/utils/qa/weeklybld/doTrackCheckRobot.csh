#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwdev" ) then
 echo "error: doTrackCheckRobot.csh must be executed from hgwdev!"
 exit 1
endif

# run on hgwdev, but using hgwbeta HG.CONF settings for the database
set log = v${BRANCHNN}.TrackCheck.log
setenv HGDB_CONF $HOME/.hg.conf.beta
nohup $WEEKLYBLD/trackCheckRobot.py $WEEKLYBLD/trackCheck.props >& $WEEKLYBLD/logs/$log
# tail -f $WEEKLYBLD/logs/$log
# egrep -i "not|warn|err" $WEEKLYBLD/logs/$log

echo "TrackCheck robot done. Check to see if any errors in $WEEKLYBLD/logs/$log."

# imperfect error test.
set wc = `cat $WEEKLYBLD/logs/$log | egrep "Error" | wc -w` 
if ( "$wc" != "0" ) then
  echo "At least 1 error may have occurred.  Check $WEEKLYBLD/logs/$log." | mail -s "v${BRANCHNN} TrackCheck robot done." ${BUILDMEISTEREMAIL} browser-qa@soe.ucsc.edu
else
   echo "Check to see if any errors in $WEEKLYBLD/logs/$log." | mail -s "v${BRANCHNN} TrackCheck robot done." ${BUILDMEISTEREMAIL} browser-qa@soe.ucsc.edu
endif

exit 0

