#!/bin/tcsh

if ($#argv < 3 || $#argv > 4) then
 echo ""
 echo "  compares trackDb on two machines."
 echo "  optionally compares another field instead of tableName."
 echo "  this will break when hgText is replaced by hgTables."
 echo
 echo "    usage: machine1 machine2 database [field] (defaults to tableName)"
 echo ""
 exit 1
endif

#set machine1 = "hgwdev"
#set machine1 = "hgwbeta"
#set machine2 = "hgw1"

set machine1 = $argv[1]
set machine2 = $argv[2]
set db = $argv[3]
set cent=""
set host=""

# check validity of machine name and existence of db on the machine
foreach machine ( $machine1 $machine2 )
  checkMachineName.csh $machine
  if ( $status ) then
    echo ${0}:
    $0
    exit 1
  endif
  if ( $machine == "hgwdev" ) then
    set cent="test"
    set host=""
  else
    if ( $machine == "hgwbeta" ) then
      set cent="beta"
      set host="-h hgwbeta"
    else
      set cent=""
      set host="-h genome-centdb"
    endif
  endif
  hgsql -N $host -e "SELECT name FROM dbDb" hgcentral$cent | grep "$db" >& /dev/null
  if ( $status ) then
    echo
    echo "  database $db is not found on $machine"
    echo
    echo ${0}:
    $0
    exit 1
  endif
  
end

checkMachineName.csh $machine1
if ( $status ) then
  echo ${0}:
  $0
  exit 1
endif

checkMachineName.csh $machine2
if ( $status ) then
  echo ${0}:
  $0
  exit 1
endif

set table = "trackDb"
set field = "tableName"

if ( $#argv == 4 ) then
  # check if valid field -- checks on dev"
  set field = $argv[4]
  hgsql -t -e "DESC $table" $db | grep -w $field | wc -l > /dev/null
  if ( $status ) then
    echo
    echo "  $field is not a valid field for $table"
    echo
    echo ${0}:
    $0
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
foreach mach ( $machine1 $machine2 )
  set url="$url1$mach$url2$db$url3$db$url4$table$url5$url6a$url6$url7"
  wget -q -O $mach.$db.$table "$url"
end

diff $machine1.$db.$table $machine2.$db.$table
if ( $status ) then
  echo "\nThe differences above are found in $table.$field"
  echo "between $machine1 and $machine2\n"
else
  echo "\n  No differences in $db.$table.$field \n  between $machine1 and $machine2 "
  echo
endif

# clean up
rm -f $machine1.$db.$table 
rm -f $machine2.$db.$table 
