#!/bin/tcsh
# This program retires the separate gateway table used for staging a new assembly in qapushq.
#  This will check and set things and then migrate the records into the real main pushQ table.
#  A backup of both the gateway table and the pushQ will be made in the current dir for safety.
#  Then if everything is ok, the separate gateway table is dropped.

if ( "$HOST" != "hgwdev" ) then
 echo "Error: this script must be run from hgwdev."
 exit 1
endif
  

if ($1 == "") then
 echo "no table specified. usage: $0 <tablename>"
 exit 1
endif

# cd ~

set host = "hgwbeta"
set hhost = "-h $host"
# just for testing, use dev:
#set host = "hgwdev"
#set hhost = ""

echo "host=$host"
echo "hhost=$hhost"

hgsqldump $hhost qapushq pushQ > pushQ.bkp
if ( $status ) then
 echo "unexpected error backing up table ${host}:qapushq.pushQ"
 exit 1
endif
echo "backed up pushQ ok."

hgsqldump $hhost qapushq $1 > $1.bkp
if ( $status ) then
 echo "unexpected error backing up table ${host}:qapushq.$1"
 exit 1
endif
echo "backed up $1 ok."

set maxqid = `hgsql $hhost qapushq -B --skip-column-names -e "select max(qid) from pushQ"`
echo "maxqid = $maxqid"
#@ x = $res + 1
#echo $x

set minqid = `hgsql $hhost qapushq -B --skip-column-names -e "select min(qid) from $1"`
echo "minqid = $minqid"
@ maxqid = ($maxqid - $minqid) + 1
echo "adjusted maxqid=$maxqid"

set sql = "update $1 set releaseLog='' where dbs='$1'"
echo $sql
hgsql $hhost qapushq -e "$sql" 
if ( $status ) then
 echo "unexpected error clearing out releaseLog field for dbs=$1 rows. ${host}:qapushq.$1 "
 exit 1
endif

set sql = "update $1 set pushState='D'"
echo $sql
hgsql $hhost qapushq -e "$sql" 
if ( $status ) then
 echo "unexpected error setting pushState=D for all rows. ${host}:qapushq.$1 "
 exit 1
endif

set sql = "update $1 set priority='L'"
echo $sql
hgsql $hhost qapushq -e "$sql" 
if ( $status ) then
 echo "unexpected error setting priority=L for all rows. ${host}:qapushq.$1 "
 exit 1
endif

set sql = "update $1 set rank=0"
echo $sql
hgsql $hhost qapushq -e "$sql" 
if ( $status ) then
 echo "unexpected error clearing out rank field for all rows. ${host}:qapushq.$1 "
 exit 1
endif

set now=`date +%Y-%m-%d`
set sql = "update $1 set qadate='$now'"
echo $sql
hgsql $hhost qapushq -e "$sql" 
if ( $status ) then
 echo "unexpected error setting qadate to today for all rows. ${host}:qapushq.$1 "
 exit 1
endif


set sql = "update $1 set qid=lpad(qid+$maxqid,6,'0')"
echo $sql
hgsql $hhost qapushq -e "$sql" 
if ( $status ) then
 echo "unexpected error setting qid for all rows. ${host}:qapushq.$1 "
 exit 1
endif

set sql = "update $1 set pqid=lpad(pqid+$maxqid,6,'0') where pqid > 0"
echo $sql
hgsql $hhost qapushq -e "$sql" 
if ( $status ) then
 echo "unexpected error setting pqid for all rows. ${host}:qapushq.$1 "
 exit 1
endif

# NOTE: the asterix in the next line must not be caught by the shell.
set sql = "insert into pushQ select * from $1"
echo "$sql"
hgsql $hhost qapushq -e "$sql"
if ( $status ) then
 echo "unexpected error adding new entries to main Q. ${host}:qapushq.$1 "
 exit 1
endif

set sql = "drop table $1"
echo $sql
hgsql $hhost qapushq -e "$sql" 
if ( $status ) then
 echo "unexpected error dropping $1 table. ${host}:qapushq.$1 "
 exit 1
endif

echo "Success! $1 records now transferred into main pushQ."
echo "$1 has been dropped.  Backups are pushQ.bkp and $1.bkp"

exit 0
