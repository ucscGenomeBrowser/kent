#!/bin/tcsh
cd $WEEKLYBLD

if ("$HOST" != "hgwbeta") then
 echo "error: you must run this script on hgwbeta!"
 exit 1
endif

# build 64 bit userApps and push it to hgdownload

set ScriptStart=`date`
echo
echo  "NOW STARTING 64-BIT userApps BUILD ON hgwbeta [${0}: `date`]"
echo
ssh -n hgwbeta $WEEKLYBLD/makeUserApps.csh
if ( $status ) then
    echo "build 64-bit userApps on $HOST failed"
    exit 1
endif
echo "Build 64-BIT userApps complete  [${0}: START=${ScriptStart} END=`date`]"



echo
echo "hgdownload userApps utils scp'd to hgdownload and hgdownload-sd"
#
exit 0

