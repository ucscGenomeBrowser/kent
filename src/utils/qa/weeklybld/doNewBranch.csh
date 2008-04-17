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

#echo debug: disabled ok cleanup
if ( -e CvsReports.ok ) then
    rm CvsReports.ok
endif
if (-e 32bitUtils.ok) then
    rm 32bitUtils.ok
endif

#echo debug: disabled cgiVersion
./updateCgiVersion.csh real

if ( $status ) then
 echo "cvs-cgi-version-update failed on $HOST"
 exit 1
endif
echo "cvs-cgi-version-update done on $HOST"


echo
#echo debug: disabled tagging
./tagNewBranch.csh real
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
#echo debug: disabled buildCvsReports
ssh hgwdev $WEEKLYBLD/buildCvsReports.csh branch real >& doNewCvs.log &
# note - we are now running it in the background on hgwdev

#---------------------

echo
echo  "NOW STARTING 32-BIT BUILD ON $BOX32 IN PARALLEL"
echo
#echo debug: disabled parallel build 32bit utils on dev
ssh $BOX32 "$WEEKLYBLD/doNewBranch32.csh opensesame" >& doNew32.log &

echo
#unpack the new branch on BUILDDIR for beta
#echo debug: disabled coBranch.csh
./coBranch.csh
if ( $status ) then
 exit 1
endif

echo
# build branch sandbox on beta
#echo debug: disabled build branch sandbox on beta
./buildBeta.csh
if ( $status ) then
 echo "build on beta failed for v$BRANCHNN"
# echo "v$BRANCHNN build on beta failed." | mail -s "'v$BRANCHNN Build failed on beta'" $USER hartera galt kuhn ann kayla rhead pauline 
echo "v$BRANCHNN build on beta failed." | mail -s "'v$BRANCHNN Build failed on beta'" $USER
 exit 1
endif
echo "build on beta done for v$BRANCHNN"
echo "v$BRANCHNN built successfully on beta." | mail -s "'v$BRANCHNN Build complete on beta'" $USER hartera galt kuhn kent ann kayla rhead pauline 

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

if ( -e CvsReports.ok ) then
    echo "CVS Reports finished ok."
else
    echo "CVS Reports had some error, no ok file found."
endif
if (-e 32bitUtils.ok) then
    echo "32-bit utils build finished ok."
else
    echo "32-bit utils build had some error, no ok file found."
endif


# do not do this here this way - we might as well wait until
#  later in the week to run this after most branch-tag moves will have been applied.
# Besides it does not work to run this script
#  from beta because when ssh wants the password
#  it fails to get input from stdin redirected across machines,
#  so that means actually logging into $BOX32 and then running it.
#  But it will take care of the rest.
#ssh $BOX32 $WEEKLYBLD/buildCgi32.csh
if ( $status ) then
    echo "build 32-bit CGIs on $BOX32 failed"
    exit 1
endif

exit 0
