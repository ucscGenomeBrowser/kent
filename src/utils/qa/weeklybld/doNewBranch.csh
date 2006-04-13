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

#cleanup
rm -f mbt*.txt

#echo debug: disabled cgiVersion
./updateCgiVersion.csh real

if ( $status ) then
 echo "cvs-cgi-version-update failed on $HOST"
 exit 1
endif
echo "cvs-cgi-version-update done on $HOST"

echo

echo
#echo debug: disabled tagging
./tagNewBranch.csh real
if ( $status ) then
 echo "tagNewBranch.csh failed on $HOST"
 exit 1
endif
echo "tagNewBranch.csh done on $HOST"
echo "new branch v$BRANCHNN created."

#echo debug: disabled buildCvsReports
./buildCvsReports.csh branch real

if ( $status ) then
 echo "buildCvsReports.csh  failed on $HOST"
 exit 1
endif
echo "buildCvsReports.csh done on $HOST"

#---------------------

echo
#unpack the new branch on BUILDDIR for beta
# This faster to co on kkstore and should use -d hgwdev:$CVSROOT
#echo debug: disabled coBranch.csh
ssh kkstore $WEEKLYBLD/coBranch.csh
if ( $status ) then
 exit 1
endif

echo
# build branch sandbox on beta
ssh hgwbeta $WEEKLYBLD/buildBeta.csh
if ( $status ) then
 echo "build on beta failed for v$BRANCHNN"
 echo "v$BRANCHNN build on beta failed." | mail -s "'v$BRANCHNN Build failed on beta'" galt heather kuhn ann kayla rhead
 exit 1
endif
echo "build on beta done for v$BRANCHNN"
echo "v$BRANCHNN built successfully on beta." | mail -s "'v$BRANCHNN Build complete on beta'" galt heather kuhn kent ann kayla rhead

echo
./doRobots.csh
if ( $status ) then
    echo "One or more of the robots had errors.  See the logs."
else
    echo "Robots done. No errors reported."
endif

echo
ssh hgwbeta $WEEKLYBLD/buildUtils.csh
if ( $status ) then
 exit 1
endif

echo
buildUtils.csh
if ( $status ) then
 exit 1
endif

exit 0
