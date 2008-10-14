#!/bin/tcsh
cd $WEEKLYBLD
if ( "$HOST" != "hgwbeta" ) then
 echo "error: doRobots.csh must be executed from hgwbeta!"
 exit 1
endif

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

echo "disabled doSearchRobot.csh [${0}: `date`]"
#./doSearchRobot.csh
#set err = $status
#if ( $err ) then
#    echo "error running doSearchRobot.csh: $err [${0}: `date`]" 
#    set returnCode=1
#endif


# note TrackCheck and LiftOver robots use java and ant, so it will not work on beta, 
#  so run them from hgwdev instead.

# need to create a mini-sandbox to build these robot utilities
#echo "disabled doJavaUtilBuild.csh [${0}: `date`]"
ssh hgwdev $WEEKLYBLD/doJavaUtilBuild.csh
set err = $status
if ( $err ) then
    echo "error running doJavaUtilBuild.csh: $err [${0}: `date`]" 
    set returnCode=1
endif 

#echo "disabled doTrackCheckRobot.csh [${0}: `date`]"
ssh hgwdev $WEEKLYBLD/doTrackCheckRobot.csh
set err = $status
if ( $err ) then
    echo "error running doTrackCheckRobot.csh: $err [${0}: `date`]" 
    set returnCode=1
endif 

# note this uses java and ant, so it will not work on beta, so run from dev instead
#echo "disabled doLiftOverTestRobot.csh [${0}: `date`]"
ssh hgwdev $WEEKLYBLD/doLiftOverTestRobot.csh
set err = $status
if ( $err ) then
    echo "error running doLiftOverTestRobot.csh: $err [${0}: `date`]" 
    set returnCode=1
endif 

echo "Done running robots TrackCheck, LiftOverTest, hgNearTest, and hgTablesTest. [${0}: `date`] "
./unsymtrick.csh

exit $returnCode

