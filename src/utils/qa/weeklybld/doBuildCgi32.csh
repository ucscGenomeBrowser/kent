#!/bin/tcsh
cd $WEEKLYBLD

if ( "$HOST" != "hgwbeta" ) then
 echo "Error: this script must be run from hgwbeta."
 exit 1
endif

set ScriptStart=`date`

echo "BRANCHNN=$BRANCHNN"
echo "TODAY=$TODAY       (last build day)"
echo "LASTWEEK=$LASTWEEK   (previous build day)"
echo "REVIEWDAY=$REVIEWDAY   (review day, day2)"
echo "LASTREVIEWDAY=$LASTREVIEWDAY   (previous review day, day2)"

if ( "$TODAY" == "" ) then
 echo "TODAY undefined."
 exit 1
endif
if ( "$BRANCHNN" == "" ) then
 echo "BRANCHNN undefined."
 exit 1
endif
if ( "$LASTWEEK" == "" ) then
 echo "LASTWEEK undefined."
 exit 1
endif
if ( "$REVIEWDAY" == "" ) then
 echo "REVIEWDAY undefined."
 exit 1
endif
if ( "$LASTREVIEWDAY" == "" ) then
 echo "LASTREVIEWDAY undefined."
 exit 1
endif

echo
echo  "NOW STARTING 32-BIT CGI BUILD ON $BOX32 [${0}: `date`]"
echo

ssh -n $BOX32 $WEEKLYBLD/buildCgi32.csh
if ( $status ) then
    echo "build 32-bit CGIs on $HOST failed"
    exit 1
endif

echo "Build 32-bit CGIs complete  [${0}: START=${ScriptStart} END=`date`]"

exit 0
