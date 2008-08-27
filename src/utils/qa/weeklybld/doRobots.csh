#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwbeta" ) then
 echo "error: doRobots.csh must be executed from hgwbeta!"
 exit 1
endif

set branch=v${BRANCHNN}_branch

if ( -d ~/bin/$MACHTYPE.orig ) then
 echo "restoring from last failed symlink."
 ./unsymtrick.csh
endif
if ( ! -d ~/bin/$MACHTYPE.cluster ) then
 echo "something messed up in symlink"
 exit 1
endif

set returnCode=0

# Symlink Trick safe now
echo "Symlink Trick."
./symtrick.csh

#echo "disabled doHgNearTestRobot.csh"
./doHgNearTestRobot.csh
set err = $status
if ( $err ) then
    echo "error running doHgNearTestRobot.csh: $err" 
    set returnCode=1
endif

#echo "disabled doHgTablesTestRobot.csh"
./doHgTablesTestRobot.csh
set err = $status
if ( $err ) then
    echo "error running doHgTablesTestRobot.csh: $err" 
    set returnCode=1
endif

echo "disabled doSearchRobot.csh"
#./doSearchRobot.csh
#set err = $status
#if ( $err ) then
#    echo "error running doSearchRobot.csh: $err" 
#    set returnCode=1
#endif


# note TrackCheck and LiftOver robots use java and ant, so it will not work on beta, 
#  so run them from hgwdev instead.

# need to create a mini-sandbox to build these robot utilities
#echo "disabled doJavaUtilBuild.csh"
ssh hgwdev $WEEKLYBLD/doJavaUtilBuild.csh
set err = $status
if ( $err ) then
    echo "error running doJavaUtilBuild.csh: $err" 
    set returnCode=1
endif 

#echo "disabled doTrackCheckRobot.csh"
ssh hgwdev $WEEKLYBLD/doTrackCheckRobot.csh
set err = $status
if ( $err ) then
    echo "error running doTrackCheckRobot.csh: $err" 
    set returnCode=1
endif 

# note this uses java and ant, so it will not work on beta, so run from dev instead
#echo "disabled doLiftOverTestRobot.csh"
ssh hgwdev $WEEKLYBLD/doLiftOverTestRobot.csh
set err = $status
if ( $err ) then
    echo "error running doLiftOverTestRobot.csh: $err" 
    set returnCode=1
endif 

echo "Done running robots TrackCheck, LiftOverTest, hgNearTest, and hgTablesTest."
./unsymtrick.csh

exit $returnCode

