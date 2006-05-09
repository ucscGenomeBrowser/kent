#!/bin/tcsh
#
# WARNING: this does not like to be run with final & to detach from terminal.
#  Heather says perhaps running with "nohup" in front of the command might
#  make it work better.
#
if ( "$HOST" != "hgwbeta" ) then
 echo "Error: this script must be run from hgwbeta."
 exit 1
endif

cd $WEEKLYBLD

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

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

echo
echo "Now beginning to build new branch $BRANCHNN"

echo

echo debug: disabled cgiVersion
#./updateCgiVersion.csh real

if ( $status ) then
 echo "cvs-cgi-version-update failed on $HOST"
 exit 1
endif
echo "cvs-cgi-version-update done on $HOST"


echo
echo debug: disabled tagging
#./tagNewBranch.csh real
if ( $status ) then
 echo "tagNewBranch.csh failed on $HOST"
 exit 1
endif
echo "tagNewBranch.csh done on $HOST"
echo "new branch v$BRANCHNN created."

echo
echo
echo  "NOW STARTING CVS-Reports ON HGWDEV IN PARALLEL"
echo
echo debug: disabled buildCvsReports
#ssh hgwdev $WEEKLYBLD/buildCvsReports.csh branch real &
# note - we are not trying running it in the background on dev
if ( $status ) then
 echo "buildCvsReports.csh  failed on $HOST"
 exit 1
endif
echo "buildCvsReports.csh done on $HOST"

#---------------------

echo
echo  "NOW STARTING 32-BIT BUILD ON HGWDEV IN PARALLEL"
echo
echo debug: disabled parallel build 32bit utils on dev
#ssh hgwdev "$WEEKLYBLD/doNewBranch32.csh opensesame" &

echo
#unpack the new branch on BUILDDIR for beta
echo debug: disabled coBranch.csh
#./coBranch.csh
if ( $status ) then
 exit 1
endif

echo
# build branch sandbox on beta
#echo debug: disabled build branch sandbox on beta
./buildBeta.csh
if ( $status ) then
 echo "build on beta failed for v$BRANCHNN"
 echo "v$BRANCHNN build on beta failed." | mail -s "'v$BRANCHNN Build failed on beta'" galt heather kuhn ann kayla rhead
 exit 1
endif
echo "build on beta done for v$BRANCHNN"
echo "v$BRANCHNN built successfully on beta." | mail -s "'v$BRANCHNN Build complete on beta'" galt heather kuhn kent ann kayla rhead

echo
#echo debug: disabled build utils on beta
./buildUtils.csh
if ( $status ) then
    exit 1
endif

echo
#echo debug: disabled doRobots
./doRobots.csh
if ( $status ) then
    echo "One or more of the robots had errors.  See the logs."
else
    echo "Robots done. No errors reported."
endif


# do not do this here this way - we might as well wait until
#  later in the week to run this after most branch-tag moves will have been applied.
#ssh hgwdev $WEEKLYBLD/buildCgis.csh
if ( $status ) then
    echo "build 32-bit CGIs on hgwdev failed"
    exit 1
endif

exit 0
