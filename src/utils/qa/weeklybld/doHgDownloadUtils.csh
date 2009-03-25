#!/bin/tcsh
cd $WEEKLYBLD

if ("$HOST" != "hgwbeta") then
 echo "error: you must run this script on hgwbeta!"
 exit 1
endif

# MAKE LIFTOVER WITHOUT SSL

# no need to do these here now, as below we have special script for making liftOver 
#scp -p /cluster/bin/i386/liftOver qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/liftOver.linux.i386
#scp -p /cluster/bin/x86_64/liftOver qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/liftOver.linux.x86_64

# build 64 and 32 bit liftOver without ssl and push it to hgdownload

set ScriptStart=`date`
echo
echo  "NOW STARTING 64-BIT liftOver without ssl BUILD ON hgwbeta [${0}: `date`]"
echo
ssh -n hgwbeta $WEEKLYBLD/makeLiftOver.csh
if ( $status ) then
    echo "build 64-bit liftOver without ssl on $HOST failed"
    exit 1
endif
echo "Build 64-BIT liftOver without ssl complete  [${0}: START=${ScriptStart} END=`date`]"


set ScriptStart=`date`
echo
echo  "NOW STARTING 32-BIT liftOver without ssl BUILD ON $BOX32 [${0}: `date`]"
echo
ssh -n $BOX32 $WEEKLYBLD/makeLiftOver.csh
if ( $status ) then
    echo "build 32-BIT liftOver without ssl on $BOX32 failed"
    exit 1
endif
echo "Build 32-BIT liftOver without ssl complete  [${0}: START=${ScriptStart} END=`date`]"


echo
echo "hgdownload utils (liftOver) scp'd to hgdownload"
#
exit 0

