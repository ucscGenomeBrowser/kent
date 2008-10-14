#!/bin/tcsh
if ( "$HOST" != "hgwbeta" ) then
 echo "Error: this script must be run from hgwbeta. [${0}: `date`]"
 exit 1
endif

cd $WEEKLYBLD

echo "updating CGI version [${0}: `date`]"

echo "BRANCHNN=$BRANCHNN"
echo "TODAY=$TODAY"
echo "LASTWEEK=$LASTWEEK"

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

cd $WEEKLYBLD/hiding

if ( -d cgiVersion ) then
 rm -fr cgiVersion
endif

cvs -d hgwdev:$CVSROOT co -d cgiVersion kent/src/hg/inc/versionInfo.h
if ( $status ) then
 echo "cvs check-out failed for versionInfo.h on $HOST [${0}: `date`]"
 exit 1
endif



cd cgiVersion

echo "Current version:"
cat versionInfo.h

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm. [${0}: `date`]"
	echo
	exit 0
endif 

echo
echo "now committing new version# into source code versionInfo.h [${0}: `date`]"

# -- set the new version # using .cshrc value BRANCHNN --
set temp = "#define CGI_VERSION "'"'"$BRANCHNN"'"'
echo $temp > versionInfo.h
cat versionInfo.h
set temp = '"'"New version number v$BRANCHNN"'"'
cvs commit -m "$temp" versionInfo.h
if ( $status ) then
 echo "cvs commit failed for versionInfo.h with new version# on $HOST [${0}: `date`]"
 exit 1
endif
echo "cvs commit done for versionInfo.h with new version# on $HOST [${0}: `date`]"

exit 0

