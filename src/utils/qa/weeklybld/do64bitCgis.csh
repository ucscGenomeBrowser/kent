#!/bin/tcsh
#
# WARNING: this does not like to be run with final & to detach from terminal.
#  Heather says perhaps running with "nohup" in front of the command might
#  make it work better.
#
if ( "$HOST" != "hgwdev" ) then
 echo "Error: this script must be run from hgwdev."
 exit 1
endif

cd $WEEKLYBLD

echo "BRANCHNN=$BRANCHNN"
echo "TODAY=$TODAY       (last build day)"
echo "LASTWEEK=$LASTWEEK   (previous build day)"

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

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

echo
echo "Now beginning to build 64 bit CGIs on kolossus"

echo
ssh kolossus $WEEKLYBLD/buildKolossus.csh 
if ( $status ) then
 exit 1
endif

echo
echo "Now restoring libs to 32 bit in branch sandbox"
# note - doing this just to make it easier for people
#  who have to move branch tag or do patching on 32bit cgis
cd /cluster/bin/build/v${BRANCHNN}_branch/kent/src/
make libs
cd $WEEKLYBLD

exit 0

