#!/bin/tcsh

if (( $1 == "" ) || ( $2 == "" ) || ( $3 == "" ) ) then
 echo ""
 echo "please specify two machines and assembly to compare their hgFindSpec files."
 echo "example> $0 hgwbeta hgw1 hg16"
 echo ""
 exit 1
endif

#set machine1 = "hgwdev"
#set machine1 = "hgwbeta"
#set machine2 = "hgw1"

set machine1 = $1
set machine2 = $2

#set db = "hg16"
set db = $3

set table = "hgFindSpec"
set field = "searchName"

#set url = "http://hgwdev.cse.ucsc.edu/cgi-bin/hgText?db=hg16&table=hg16.trackDb&outputType=Tab-separated%2C+Choose+fields...&origPhase=Get+results&field_tableName=on&phase=Get+these+fields"

set url1 = "http://"
set url2 = ".cse.ucsc.edu/cgi-bin/hgText?db="
set url3 = "&table="
set url4 = "."
set url5 = "&outputType=Tab-separated%2C+Choose+fields...&origPhase=Get+results&field_"
set url6 = "=on&phase=Get+these+fields"

rm -f $machine1.$db.$table 
rm -f $machine2.$db.$table 

set machine = "$machine1"
wget -q -O $machine.$db.$table "$url1$machine$url2$db$url3$db$url4$table$url5$field$url6"

set machine = "$machine2"
wget -q -O $machine.$db.$table "$url1$machine$url2$db$url3$db$url4$table$url5$field$url6"

"diff" $machine1.$db.$table $machine2.$db.$table
if ( $status ) then
 echo "Differences in $table found between $machine1 and $machine2"
else
 echo "No differences in $table."
endif
