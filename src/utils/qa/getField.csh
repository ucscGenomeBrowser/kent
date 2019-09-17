#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  01-26-07
#  Robert Kuhn
#
#  gets a column of table data from any node using wget
#
################################

set db=""
set table=""
set field=""
set machine="hgw1"

if ( $#argv < 3 || $#argv > 4 ) then
  echo
  echo "  gets a column of table data from any node using wget"
  echo
  echo "    usage:  database table field [machine]"
  echo "             (defaults to hgw1)"
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
  set field=$argv[3]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

if ( 4 == $#argv ) then
  set machine=$argv[4]
endif

set url1="http://"
# $machine = hgwdev-kuhn
set url2=".soe.ucsc.edu/cgi-bin/hgTables?db="
# $db = hg17
set url3="&hgta_outputType=selectedFields&hgta_regionType=genome&hgta_table="
# $table = trackDb
set url4="&outputType=Tab-separated%2C+Choose+fields...&origPhase=Get+results"
set url5="&hgta_group=allTables&field_tableName=on"
set url6="&hgta_fs.check.$db.$table.$field=1&hgta_database=$db"
set url7="&hgta_fieldSelectTable=$db.$table"
set url8="&phase=Get+these+fields&hgta_doPrintSelectedFields=get+output"

set url="$url1$machine$url2$db$url3$table$url4$url5$url6$url7$url8"

wget -q -O /dev/stdout "$url" | sed -e "1d"

