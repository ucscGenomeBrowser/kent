#!/bin/tcsh
source `which qaConfig.csh`
##########################################################
# remove unneeded trackDb_ and hgFindSpec_ tables        #
#  on hgwbeta for given assembly ($1)                    #
##########################################################

if ( "$HOST" != "hgwdev" ) then
    echo "error: you must run this script on hgwdev!"
    exit 1
endif

set host = "hgwbeta"
    
# check cmdline parm1 (db)
if ("$1" == "") then
    echo "Usage Error: specify on commandline the database to use."
    echo "e.g. dropUserTables.csh droMoj1"
    exit 1
endif
set db = "$1"

set sql = "show tables like "'"'"trackDb_%"'"'
#echo "$sql"
set r = `hgsql -h $host $db -BN -e "$sql"`
if ( $status ) then
    echo "Error executing sql: $sql."
    exit 1
endif
#echo $r

set tables = ($r)

set sql = "show tables like "'"'"hgFindSpec_%"'"'
#echo "$sql"
set r = `hgsql -h $host $db -BN -e "$sql"`
if ( $status ) then
    echo "Error executing sql: $sql."
    exit 1
endif
#echo $r

set tables = ($tables $r)

if ("$2" != "real") then
    echo "\nFound these tables: \n $tables\n"
    echo "Must specify 'real' on commandline to actually do it."
    exit 0
endif

foreach table ($tables)
    set sql = "drop table $table"
    echo "$sql"
    hgsql -h $host $db -e "$sql" 
    if ( $status ) then
	echo "Error running query: $sql."
    	exit 1
    endif
end    

exit 0

