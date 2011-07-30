#!/bin/tcsh
cd $WEEKLYBLD

if ( "$HOST" != "hgwdev" ) then
	echo "error: you must run this script on hgwdev!"
	exit 1
endif

set currentBranch=`git branch | grep master`
if ("$currentBranch" != "* master") then
    echo "Error: must be on master branch"
    exit 1
endif

# This should be in hgdownload sandbox
cd ${BUILDHOME}/build-hgdownload/admin
git pull origin master

hgsqldump --all -d -c -h genome-centdb hgcentral \
sessionDb userDb hubStatus | sed -e "s/genome-centdb/localhost/" > \
/tmp/hgcentraltemp.sql

# --skip-extended-insert 
#   to make it dump rows as separate insert statements
hgsqldump --all --skip-extended-insert -c -h genome-centdb hgcentral \
defaultDb blatServers dbDb dbDbArch gdbPdb liftOverChain clade genomeClade targetDb hubPublic | \
sed -e "s/genome-centdb/localhost/" >> /tmp/hgcentraltemp.sql


# get rid of some mysql5 trash in the output we don't want.
# also need to break data values at rows so the diff and cvs 
# which are line-oriented work better.
grep -v "Dump completed on" /tmp/hgcentraltemp.sql | \
sed -e "s/AUTO_INCREMENT=[0-9]* //" > \
/tmp/hgcentral.sql

echo
echo "*** Diffing old new ***"
diff hgcentral.sql /tmp/hgcentral.sql
if ( ! $status ) then
	echo
	echo "No differences."
	echo
	exit 0
endif 

if ( "$1" != "real" ) then
	echo
	echo "Not real.   To make real changes, put real as cmdline parm."
	echo
	exit 0
endif 

rm hgcentral.sql
cp -p /tmp/hgcentral.sql hgcentral.sql
set temp = '"'"v${BRANCHNN}"'"'
git commit -m $temp hgcentral.sql
if ( $status ) then
	echo "error during git commit of hgcentral.sql."
	exit 1
endif

# push to hgdownload
ssh -n qateam@hgdownload "rm /mirrordata/apache/htdocs/admin/hgcentral.sql"
scp -p hgcentral.sql qateam@hgdownload:/mirrordata/apache/htdocs/admin/

echo
echo "A new hgcentral.sql file should now be present at:"
echo "  http://hgdownload.cse.ucsc.edu/admin/"
echo
echo "If it is not, you can request a push of the file:"
echo "  /usr/local/apache/htdocs/admin/hgcentral.sql"
echo "  from hgwdev --> hgdownload "
echo
echo "NOTE:  Some mirrors like to get hgcentral tables via ftp or rsync"
echo "from hgdownload.cse.ucsc.edu/mysql/hgcentral/ instead of from the"
echo "hgcentral.sql file. To make a table in hgcentral available there"
echo "right now, ask for it to be pushed from hgnfs1 --> hgdownload. (Or"
echo "just wait for the automatic weekly rsync.)"
echo

git pull
git push

exit 0

