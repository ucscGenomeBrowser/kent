#!/bin/tcsh
cd $WEEKLYBLD

if ("$HOST" != "hgwbeta") then
 echo "error: you must run this script on hgwbeta!"
 exit 1
endif

#9403# # build 64 and 32 bit userApps and push it to hgdownload
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


#9403# set ScriptStart=`date`
#9403# echo
#9403# echo  "NOW STARTING 32-BIT userApps BUILD ON $BOX32 [${0}: `date`]"
#9403# echo
#9403# ssh -n $BOX32 $WEEKLYBLD/makeUserApps.csh
#9403# if ( $status ) then
#9403#     echo "build 32-BIT userApps on $BOX32 failed"
#9403#     exit 1
#9403# endif
#9403# echo "Build 32-BIT userApps complete  [${0}: START=${ScriptStart} END=`date`]"


echo
echo "hgdownload userApps utils scp'd to hgdownload and hgdownload-sd"
#
exit 0

