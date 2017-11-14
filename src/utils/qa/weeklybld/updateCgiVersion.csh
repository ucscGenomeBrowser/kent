#!/bin/tcsh
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

cd $WEEKLYBLD
cd ../../../../src/hg/inc/

echo "Current version:"
grep CGI_VERSION versionInfo.h

set currentBranch=`git branch | grep master`
if ("$currentBranch" != "* master") then
    echo "Error: must be on master branch"
    exit 1
endif

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
echo "/* Copyright (C) 2014 The Regents of the University of California \
 * See README in this or parent directory for licensing information. */" > versionInfo.h
echo $temp >> versionInfo.h
sed -e 's/CGI_VERSION/SRC_VERSION/;' versionInfo.h > ../../inc/srcVersion.h
grep CGI_VERSION versionInfo.h
set temp = "New version number v$BRANCHNN"
git add versionInfo.h ../../inc/srcVersion.h
git commit -m "$temp" versionInfo.h ../../inc/srcVersion.h
if ( $status ) then
 echo "git commit failed for versionInfo.h with new version# on $HOST [${0}: `date`]"
 exit 1
endif
echo "git commit done for versionInfo.h with new version# on $HOST [${0}: `date`]"
git pull
git push

exit 0

