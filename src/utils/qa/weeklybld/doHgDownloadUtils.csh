#!/bin/tcsh
cd $WEEKLYBLD

if ("$HOST" != "hgwbeta") then
 echo "error: you must run this script on hgwbeta!"
 exit 1
endif

scp -p /cluster/bin/i386/liftOver qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/liftOver.linux.i386
scp -p /cluster/bin/x86_64/liftOver qateam@hgdownload:/mirrordata/apache/htdocs/admin/exe/liftOver.linux.x86_64

echo
echo "hgdownload utils (liftOver) gz'd on $HOST and scp'd to hgdownload"
#
exit 0

