#!/bin/tcsh

if ($#argv < 3 || $#argv > 4) then
 echo ""
 echo "  compares trackDb on two machines."
 echo "  optionally compares another field instead of tableName."
 echo "  this will break when hgText is replaced by hgTables."
 echo
 echo "    usage: machine1, machine2, database, [field] (defaults to tableName)"
 echo ""
 exit 1
endif

#set machine1 = "hgwdev"
#set machine1 = "hgwbeta"
#set machine2 = "hgw1"

set machine1 = $argv[1]
set machine2 = $argv[2]
set db = $argv[3]

set allMachines=" hgwdev hgwbeta hgw1 hgw2 hgw3 hgw4 hgw5 hgw6 hgw7 hgw8 mgc "

set mach1Ok=`echo $allMachines | grep -w "$machine1"`
if ($status) then
  echo "\n $machine1 is not a valid machine.\n"
  echo "    usage: machine1, machine2, database, [field] (defaults to tableName)"
  exit 1
endif

set mach2Ok=`echo $allMachines | grep -w "$machine2"`
if ($status) then
  echo "\n $machine2 is not a valid machine.\n"
  echo "    usage: machine1, machine2, database, [field] (defaults to tableName)\n"
  exit 1
endif


set dbOk=`hgsql -t -e "SHOW databases" | grep " $db " | wc -l`
if ($dbOk == 0) then
  echo
  echo " $db is not a valid database on hgwdev."
  echo "    usage: machine1, machine2, database, [field] (defaults to tableName)"
  echo
  exit 1
endif


set table = "trackDb"
set field = "tableName"

if ($#argv == 4) then
  # check if valid field -- checks on dev"
  set field = $argv[4]
  set fieldOk=`hgsql -t -e "DESC $table" $db | grep " $field " | wc -l`
  if ($fieldOk == 0) then
    echo
    echo " $field is not a valid field for $table"
    echo "    usage: machine1, machine2, database, [field] (defaults to tableName)"
    echo
    exit 1
  endif
endif

#set url = "http://hgwdev.cse.ucsc.edu/cgi-bin/hgText?db=hg16&table=hg16.trackDb&outputType=Tab-separated%2C+Choose+fields...&origPhase=Get+results&field_tableName=on&phase=Get+these+fields"

set url1 = "http://"
set url2 = ".cse.ucsc.edu/cgi-bin/hgText?db="
set url3 = "&table="
set url4 = "."
set url5 = "&outputType=Tab-separated%2C+Choose+fields...&origPhase=Get+results"
set url6 = "&field_$field=on"
set url7 = "&phase=Get+these+fields"

# add tableName to output if checking other fields -- to help interpret results
# doesn't work for settings of because of embedded newlines
set url6a = ""
if ($field != "tableName") then
  set url6a = "&field_tableName=on"
endif

rm -f $machine1.$db.$table 
rm -f $machine2.$db.$table 

set machine=$machine1
wget -q -O $machine1.$db.$table "$url1$machine$url2$db$url3$db$url4$table$url5$url6a$url6$url7"
set mach1Count=`wc -l $machine1.$db.$table | gawk '{print $1}'`

set machine=$machine2
wget -q -O $machine2.$db.$table "$url1$machine$url2$db$url3$db$url4$table$url5$url6a$url6$url7"

set mach1Count=`wc -l $machine1.$db.$table | gawk '{print $1}'`
set mach2Count=`wc -l $machine2.$db.$table | gawk '{print $1}'`

if ($mach1Count == 0 | $mach2Count == 0) then
  echo "\n At least one of these machines does not have the database $db.\n"
  exit 1
endif

"diff" $machine1.$db.$table $machine2.$db.$table
if ( $status ) then
 echo "The differences above are found in $table.$field"
 echo "between $machine1 and $machine2\n"
else
  echo "\n  No differences in $db.$table.$field \n  between $machine1 and $machine2 "
  echo
endif

# clean up
rm -f $machine1.$db.$table 
rm -f $machine2.$db.$table 
