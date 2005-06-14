#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwdev" ) then
 echo "error: doRobots.csh must be executed from hgwdev!"
 exit 1
endif

set branch=v${BRANCHNN}_branch

if ( -d ~/bin/i386.orig ) then
 echo "restoring from last failed symlink."
 ./unsymtrick.csh
endif
if ( ! -d ~/bin/i386.cluster ) then
 echo "something messed up in symlink"
 exit 1
endif

set returnCode=0

# Symlink Trick safe now
echo "Symlink Trick."
./symtrick.csh

ssh hgwbeta $WEEKLYBLD/doHgNearTestRobot.csh
set err = $status
if ( $err ) then
    echo "error running doHgNearTestRobot.csh: $err" 
    set returnCode=1
endif

ssh hgwbeta $WEEKLYBLD/doHgTablesTestRobot.csh
set err = $status
if ( $err ) then
    echo "error running doHgTablesTestRobot.csh: $err" 
    set returnCode=1
endif

./doTrackCheckRobot.csh
set err = $status
if ( $err ) then
    echo "error running doTrackCheckRobot.csh: $err" 
    set returnCode=1
endif 

./doLiftOverTestRobot.csh
set err = $status
if ( $err ) then
    echo "error running doLiftOverTestRobot.csh: $err" 
    set returnCode=1
endif 

echo "Done running robots TrackCheck, LiftOverTest, hgNearTest, and hgTablesTest."
./unsymtrick.csh

exit $returnCode

