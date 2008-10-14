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
echo "Now beginning to build new branch $BRANCHNN [${0}: `date`]"

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
 echo "cvs-cgi-version-update failed on $HOST [${0}: `date`]"
 exit 1
endif
echo "cvs-cgi-version-update done on $HOST [${0}: `date`]"


echo
#echo debug: disabled tagging
./tagNewBranch.csh real
if ( $status ) then
 echo "tagNewBranch.csh failed on $HOST [${0}: `date`]"
 exit 1
endif
echo "tagNewBranch.csh done on $HOST [${0}: `date`]"
echo "new branch v$BRANCHNN created."

echo
echo
echo  "NOW STARTING CVS-Reports ON HGWDEV IN PARALLEL [${0}: `date`]"
echo
#echo debug: disabled buildCvsReports
ssh -n hgwdev $WEEKLYBLD/buildCvsReports.csh branch real >& doNewCvs.log &
# note - we are now running it in the background on hgwdev

#---------------------

echo
echo  "NOW STARTING 32-BIT BUILD ON $BOX32 IN PARALLEL [${0}: `date`]"
echo
#echo debug: disabled parallel build 32bit utils on dev
ssh -n $BOX32 "$WEEKLYBLD/doNewBranch32.csh opensesame" >& doNew32.log &

echo "Unpack the new branch on BUILDDIR for beta [${0}: `date`]"
#unpack the new branch on BUILDDIR for beta
#echo debug: disabled coBranch.csh
./coBranch.csh
if ( $status ) then
 echo "Unpack the new branch on BUILDDIR for beta FAILED [${0}: `date`]"
 exit 1
endif

echo "Build utils on beta [${0}: `date`]"
echo
#echo debug: disabled build utils on beta
./buildUtils.csh
if ( $status ) then
    echo "Build utils on beta FAILED [${0}: `date`]"
    exit 1
endif

echo
echo "Build branch sandbox on beta [${0}: `date`]"
# build branch sandbox on beta
#echo debug: disabled build branch sandbox on beta
./buildBeta.csh
if ( $status ) then
 echo "build on beta failed for v$BRANCHNN [${0}: `date`]"
# echo "v$BRANCHNN build on beta failed." | mail -s "'v$BRANCHNN Build failed on beta'" $USER galt browser-qa
echo "v$BRANCHNN build on beta failed [${0}: `date`]." | mail -s "'v$BRANCHNN Build failed on beta'" $USER
 exit 1
endif
echo "build on beta done for v$BRANCHNN [${0}: `date`]"
echo "v$BRANCHNN built successfully on beta (day 9)." | mail -s "'v$BRANCHNN Build complete on beta (day 9).'" $USER galt kent browser-qa

echo
echo "Waiting for the background beta:cvs-reports and ${BOX32}:doNewBranch32.csh to finish [${0}: `date`]"
wait
echo "Wait complete, checking results. [${0}: `date`]"

if ( -e CvsReports.ok ) then
    echo "CVS Reports finished ok. [${0}: `date`]"
else
    echo "CVS Reports had some error, no ok file found. [${0}: `date`]"
endif
if (-e 32bitUtils.ok) then
    echo "32-bit utils build finished ok. [${0}: `date`]"
else
    echo "32-bit utils build had some error, no ok file found. [${0}: `date`]"
endif

exit 0
