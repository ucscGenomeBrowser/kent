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
set CREATE_ONLY="sessionDb userDb hubStatus gbMembers namedSessionDb apiKeys"
set CREATE_OR_LIST=`echo "${CREATE_ONLY}" | sed -e "s/ /|/g"`
set IGNORE_TABLES=`hgsql -N -h genome-centdb -e "show tables;" hgcentral \
     | egrep -v -w "${CREATE_OR_LIST}" | xargs echo \
     | sed -e "s/^/--ignore-table=hgcentral./; s/ / --ignore-table=hgcentral./g"`
hgsqldump --skip-add-drop-table --skip-lock-tables --no-data ${IGNORE_TABLES} \
          -h genome-centdb --no-create-db --databases hgcentral  | grep -v "^USE " \
         | sed -e "s/genome-centdb/localhost/; s/CREATE TABLE/CREATE TABLE IF NOT EXISTS/" \
    > /tmp/hgcentraltemp.sql 

### Creates and fills (replacing entirely) these tables
set REPLACE_ENTIRELY="blatServers dbDb defaultCart liftOverChain quickLiftChain asmAlias assemblyList genark genarkOrg"
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
#
# These tables are kept on the mirror (CREATE TABLE IF NOT EXISTS), so a column
# we add on the RR never reaches a mirror whose table predates it, and the
# REPLACE rows below, which name every column, then fail with "Unknown column"
# and stop the whole load (refs #38503: hubPublic.email). So the dump is written
# in three parts: the table definitions, then for every column of these tables
# a statement that adds the column only if the mirror's table lacks it, then
# the rows. The add-if-missing check uses information_schema and a prepared
# statement rather than ADD COLUMN IF NOT EXISTS, which MySQL does not have.
# CHAR(96) is a backtick, written that way to keep it away from tcsh.
hgsqldump ${IGNORE_TABLES} --skip-lock-tables --skip-add-drop-table --no-data -h genome-centdb \
        --no-create-db --databases hgcentral  | grep -v "^USE " | sed -e \
        "s/genome-centdb/localhost/; s/CREATE TABLE/CREATE TABLE IF NOT EXISTS/" \
    >> /tmp/hgcentraltemp.sql

set TABLE_IN_LIST=`echo "${CREATE_AND_FILL}" | sed -e "s/ /','/g"`
echo "" >> /tmp/hgcentraltemp.sql
echo "-- Add any of the columns above that an older mirror table is missing" >> /tmp/hgcentraltemp.sql
hgsql -N -h genome-centdb -e "SELECT CONCAT( \
    'SET @hgcAddCol = (SELECT IF(COUNT(*) = 0, ''ALTER TABLE ', CHAR(96), TABLE_NAME, CHAR(96), \
    ' ADD COLUMN ', CHAR(96), COLUMN_NAME, CHAR(96), ' ', COLUMN_TYPE, \
    IF(IS_NULLABLE = 'YES', ' DEFAULT NULL', ' NOT NULL'), \
    ''', ''DO 0'') FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ''', \
    TABLE_NAME, ''' AND COLUMN_NAME = ''', COLUMN_NAME, '''); ', \
    'PREPARE hgcAddColStmt FROM @hgcAddCol; EXECUTE hgcAddColStmt; DEALLOCATE PREPARE hgcAddColStmt;') \
    FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = 'hgcentral' \
    AND TABLE_NAME IN ('${TABLE_IN_LIST}') ORDER BY TABLE_NAME, ORDINAL_POSITION" \
    >> /tmp/hgcentraltemp.sql
if ( $status ) then
	echo "error: could not generate the add-missing-column statements"
	exit 1
endif
echo "" >> /tmp/hgcentraltemp.sql

hgsqldump ${IGNORE_TABLES} --skip-lock-tables --no-create-info --skip-extended-insert --order-by-primary -c -h genome-centdb \
        --no-create-db --databases hgcentral  | grep -v "^USE " | sed -e \
        "s/genome-centdb/localhost/; s/INSERT/REPLACE/" \
    >> /tmp/hgcentraltemp.sql

# get rid of some mysql5 trash in the output we don't want, as well as
# the mariadbdump "sandbox mode" lines.
# also need to break data values at rows so the diff and cvs 
# which are line-oriented work better.
grep -v "Dump completed on" /tmp/hgcentraltemp.sql | \
grep -v '999999.*enable the sandbox mode' | \
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

#ssh -n qateam@hgdownload2 "rm /mirrordata/apache/htdocs/admin/hgcentral.sql"
#scp -p hgcentral.sql qateam@hgdownload2:/mirrordata/apache/htdocs/admin/

ssh -n qateam@hgdownload3 "rm /mirrordata/apache/htdocs/admin/hgcentral.sql"
scp -p hgcentral.sql qateam@hgdownload3:/mirrordata/apache/htdocs/admin/

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
echo "  genome-euro"
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

