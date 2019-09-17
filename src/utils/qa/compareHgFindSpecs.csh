#!/bin/tcsh
source `which qaConfig.csh`

if ($#argv < 3 || $#argv > 4) then
 echo ""
 echo "  compares hgFindSpec on two machines."
 echo "  optionally compares another field instead of searchName."
 echo
 echo "    usage: machine1 machine2 database [field] (defaults to searchName)"
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
# use the appropriate centdb
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
      set host="-h $sqlbeta"
    else
      set cent=""
      set host="-h $sqlrr"
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

set table = "hgFindSpec"
set field = "searchName"


if ( $#argv == 4 ) then
  # check if valid field -- checks on localhost (usually hgwdev)
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


#### NEW WAY WITH hgTables
#set url = "http://hgwdev.soe.ucsc.edu/cgi-bin/hgTables?db=hg16&hgta_outputType=selectedFields&hgta_regionType=genome&hgta_table=trackDb&origPhase=Get+results&outputType=Tab-separated%2C+Choose+fields...&phase=Get+these+fields&hgta_doPrintSelectedFields=get+output"

set url1 = "http://"
set url2 = ".soe.ucsc.edu/cgi-bin/hgTables?db="
set url3 = "&hgta_outputType=selectedFields&hgta_regionType=genome&hgta_table="
set url4 = "&outputType=Tab-separated%2C+Choose+fields...&origPhase=Get+results"
set url5 = "&field_$field=on"
set url6 = "&phase=Get+these+fields&hgta_doPrintSelectedFields=get+output"

# add tableName to output if checking other fields -- to help interpret results
# doesn't work for settings of because of embedded newlines
set url5a = ""
if ($field != "tableName") then
  set url5a = "&field_tableName=on"
endif

set machine=$machine1
foreach mach ( $machine1 $machine2 )
  set url="$url1$mach$url2$db$url3$table$url4$url5a$url5$url6"
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
