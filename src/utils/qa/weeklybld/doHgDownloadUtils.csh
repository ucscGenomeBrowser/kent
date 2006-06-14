#!/bin/tcsh
cd $WEEKLYBLD

if ("$HOST" != "hgwbeta") then
 echo "error: you must run this script on hgwbeta!"
 exit 1
endif

gzip -c /cluster/bin/i386/liftOver > liftOver.linux.i386
gzip -c /cluster/bin/x86_64/liftOver > liftOver.linux.x86_64

scp -p liftOver.linux.{i386,x86_64} qateam@hgdownload:/mirrordata/apache/htdocs/admin/

rm liftOver.linux.{i386,x86_64}

echo
echo "hgdownload utils (liftOver) gz'd on $HOST and scp'd to hgdownload"
#
exit 0

