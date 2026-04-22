#!/bin/tcsh
cd $WEEKLYBLD

set branch=v${BRANCHNN}_branch

if ( -d ~/bin/$MACHTYPE.orig ) then
 echo "restoring from last failed symlink. [${0}: `date`]"
 ./unsymtrick.csh
endif
if ( ! -d ~/bin/$MACHTYPE.cluster ) then
 echo "something messed up in symlink [${0}: `date`]"
 exit 1
endif

set returnCode=0

# Symlink Trick safe now
echo "Symlink Trick. [${0}: `date`]"
./symtrick.csh

#echo "disabled doHgNearTestRobot.csh"
./doHgNearTestRobot.csh
set err = $status
if ( $err ) then
    echo "error running doHgNearTestRobot.csh: $err [${0}: `date`]"  
    set returnCode=1
endif

#echo "disabled doHgTablesTestRobot.csh"
./doHgTablesTestRobot.csh
set err = $status
if ( $err ) then
    echo "error running doHgTablesTestRobot.csh: $err [${0}: `date`]" 
    set returnCode=1
endif

# NOTE QA asked to have this robot more or less permanently disabled.
echo "disabled doSearchRobot.csh [${0}: `date`]"
#./doSearchRobot.csh
#set err = $status
#if ( $err ) then
#    echo "error running doSearchRobot.csh: $err [${0}: `date`]" 
#    set returnCode=1
#endif


# TrackCheck and LiftOver are python-based; run from hgwdev.

#echo "disabled doTrackCheckRobot.csh [${0}: `date`]"
./doTrackCheckRobot.csh
set err = $status
if ( $err ) then
    echo "error running doTrackCheckRobot.csh: $err [${0}: `date`]" 
    set returnCode=1
endif 

python3 ./doLiftOverTestRobot.py
set err = $status
if ( $err ) then
    echo "error running doLiftOverTestRobot.py: $err [${0}: `date`]"
    set returnCode=1
endif

echo "Done running robots TrackCheck, LiftOverTest, hgNearTest, and hgTablesTest. [${0}: `date`] "
./unsymtrick.csh

exit $returnCode

