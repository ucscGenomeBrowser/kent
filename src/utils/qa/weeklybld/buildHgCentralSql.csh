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

### Creates these tables only
set CREATE_ONLY="sessionDb userDb hubStatus gbMembers namedSessionDb" 
set CREATE_OR_LIST=`echo "${CREATE_ONLY}" | sed -e "s/ /|/g"`
set IGNORE_TABLES=`hgsql -N -h genome-centdb -e "show tables;" hgcentral \
     | egrep -v -w "${CREATE_OR_LIST}" | xargs echo \
     | sed -e "s/^/--ignore-table=hgcentral./; s/ / --ignore-table=hgcentral./g"`
hgsqldump --skip-add-drop-table --skip-lock-tables --no-data ${IGNORE_TABLES} \
          -h genome-centdb --no-create-db --databases hgcentral  | grep -v "^USE " \
         | sed -e "s/genome-centdb/localhost/; s/CREATE TABLE/CREATE TABLE IF NOT EXISTS/" \
    > /tmp/hgcentraltemp.sql 

### Creates and fills (replacing entirely) these tables
set REPLACE_ENTIRELY="blatServers dbDb dbDbArch gdbPdb liftOverChain" 
set CREATE_OR_LIST=`echo "${REPLACE_ENTIRELY}" | sed -e "s/ /|/g"`
set IGNORE_TABLES=`hgsql -N -h genome-centdb -e "show tables;" hgcentral \
     | egrep -v -w "${CREATE_OR_LIST}" | xargs echo \
     | sed -e "s/^/--ignore-table=hgcentral./; s/ / --ignore-table=hgcentral./g"`
# --order-by-primary ... to make it dump rows in a stable repeatable order if it has an index
# --skip-extended-insert ... to make it dump rows as separate insert statements
# --skip-add-drop-table ... to avoid dropping existing tables
# Note that INSERT is turned into REPLACE making our table contents dominant, 
#      but users additional rows are preserved
hgsqldump ${IGNORE_TABLES} --skip-lock-tables --skip-extended-insert --order-by-primary -c -h genome-centdb \
        --no-create-db --databases hgcentral  | grep -v "^USE " | sed -e \
        "s/genome-centdb/localhost/" \
    >> /tmp/hgcentraltemp.sql

### Creates and fills (replacing uniquely keyed rows only) these tables
set CREATE_AND_FILL="defaultDb clade genomeClade targetDb hubPublic" 
set CREATE_OR_LIST=`echo "${CREATE_AND_FILL}" | sed -e "s/ /|/g"`
set IGNORE_TABLES=`hgsql -N -h genome-centdb -e "show tables;" hgcentral \
     | egrep -v -w "${CREATE_OR_LIST}" | xargs echo \
     | sed -e "s/^/--ignore-table=hgcentral./; s/ / --ignore-table=hgcentral./g"`
# --order-by-primary ... to make it dump rows in a stable repeatable order if it has an index
# --skip-extended-insert ... to make it dump rows as separate insert statements
# --skip-add-drop-table ... to avoid dropping existing tables
# Note that INSERT is turned into REPLACE making our table contents dominant, 
#      but users additional rows are preserved
hgsqldump ${IGNORE_TABLES} --skip-lock-tables --skip-add-drop-table --skip-extended-insert --order-by-primary -c -h genome-centdb \
        --no-create-db --databases hgcentral  | grep -v "^USE " | sed -e \
        "s/genome-centdb/localhost/; s/CREATE TABLE/CREATE TABLE IF NOT EXISTS/; s/INSERT/REPLACE/" \
    >> /tmp/hgcentraltemp.sql

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
ssh -n qateam@hgdownload-sd "rm /mirrordata/apache/htdocs/admin/hgcentral.sql"
scp -p hgcentral.sql qateam@hgdownload-sd:/mirrordata/apache/htdocs/admin/
ssh -n qateam@genome-euro "rm /mirrordata/apache/htdocs/admin/hgcentral.sql"
scp -p hgcentral.sql qateam@genome-euro:/mirrordata/apache/htdocs/admin/

# archive
set dateStamp = `date "+%FT%T"`
cp -p hgcentral.sql /hive/groups/browser/centralArchive/hgcentral.$dateStamp.sql
gzip /hive/groups/browser/centralArchive/hgcentral.$dateStamp.sql

echo
echo "A new hgcentral.sql file should now be present at:"
echo "  http://hgdownload.soe.ucsc.edu/admin/"
echo "   and"
echo "  http://hgdownload-sd.soe.ucsc.edu/admin/"
echo
echo "If it is not, you can request a push of the file:"
echo "  /usr/local/apache/htdocs/admin/hgcentral.sql"
echo "  from hgwdev --> hgdownload "
echo
echo "NOTE:  Some mirrors like to get hgcentral tables via ftp or rsync"
echo "from hgdownload.soe.ucsc.edu/mysql/hgcentral/ instead of from the"
echo "hgcentral.sql file. To make a table in hgcentral available there"
echo "right now, ask for it to be pushed from hgnfs1 --> hgdownload. (Or"
echo "just wait for the automatic weekly rsync.)"
echo

git pull
git push

exit 0

