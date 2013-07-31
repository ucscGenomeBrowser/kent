#!/bin/tcsh
cd $WEEKLYBLD

# build 64 bit userApps and push it to hgdownload

set ScriptStart=`date`
echo
echo  "NOW STARTING 64-BIT userApps BUILD ON hgwdev [${0}: `date`]"
echo
ssh -n hgwdev $WEEKLYBLD/makeUserApps.csh
if ( $status ) then
    echo "build 64-bit userApps on $HOST failed"
    exit 1
endif
echo "Build 64-BIT userApps complete  [${0}: START=${ScriptStart} END=`date`]"



echo
echo "hgdownload userApps utils scp'd to hgdownload and hgdownload-sd"
#
exit 0

