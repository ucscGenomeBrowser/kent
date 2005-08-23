#!/bin/csh -ef

if ($#argv != 2) then
    echo "usage: dumpTable.csh <db> <table>"
    exit 1
endif

set db = $1
set table = $2
set user = `awk -F= '/db.user=/ {print $2}' < ~/.hg.conf`
set pwd = `awk -F= '/db.password=/ {print $2}' < ~/.hg.conf`

mysqldump -u$user -p$pwd -T . $db $table
echo "Creating $db $table.sql and $table.txt"
wc -l $table.txt
exit 0
