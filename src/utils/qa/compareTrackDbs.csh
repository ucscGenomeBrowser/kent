#!/bin/tcsh

if ($#argv < 3 || $#argv > 4) then
 echo ""
 echo "  compares trackDb on two machines."
 echo "  optionally compares another field instead of tableName."
 echo "  this will break when hgText is retired."
 echo
 echo "    usage: machine1 machine2 database [field] (defaults to tableName)"
 echo ""
 exit 1
endif

set machine1 = $argv[1]
set machine2 = $argv[2]
set db = $argv[3]
set cent=""
set host=""

#set machine1 = "hgwdev"
#set machine1 = "hgwbeta"
#set machine2 = "hgw1"

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
      hgsql -N $host -e "SHOW TABLES" "$db" >& /dev/null
      if ( $status ) then
        echo
        echo "  database $db is not found on $machine"
        echo
        echo ${0}:
        $0
        exit 1
      endif
    else
      set cent=""
      set host="-h genome-centdb"
    endif
  endif

  hgsql -N $host -e "SELECT name FROM dbDb" hgcentral$cent | grep "$db" >& /dev/null
  if ( $status ) then
    echo
    echo "  database $db is not in dbDb on $machine"
    echo
    echo ${0}:
    $0
    exit 1
  endif
end

set table = "trackDb"
set field = "tableName"

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


#### OLD WAY WITH HGTEXT !!! #####
#set url = "http://hgwdev.cse.ucsc.edu/cgi-bin/hgText?db=hg16&table=hg16.trackDb&outputType=Tab-separated%2C+Choose+fields...&origPhase=Get+results&field_tableName=on&phase=Get+these+fields"

#set url1 = "http://"
#set url2 = ".cse.ucsc.edu/cgi-bin/hgText?db="
#set url3 = "&table="
#set url4 = "."
#set url5 = "&outputType=Tab-separated%2C+Choose+fields...&origPhase=Get+results"
#set url6 = "&field_$field=on"
#set url7 = "&phase=Get+these+fields"

#### NEW WAY WITH hgTables
#set url = "http://hgwdev.cse.ucsc.edu/cgi-bin/hgTables?db=hg16&hgta_outputType=selectedFields&hgta_regionType=genome&hgta_table=trackDb&origPhase=Get+results&outputType=Tab-separated%2C+Choose+fields...&phase=Get+these+fields&hgta_doPrintSelectedFields=get+output"

set url1 = "http://"
set url2 = ".cse.ucsc.edu/cgi-bin/hgTables?db="
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

rm -f $machine1.$db.$table 
rm -f $machine2.$db.$table 

set machine=$machine1
foreach mach ( $machine1 $machine2 )
  set url="$url1$mach$url2$db$url3$table$url4$url5a$url5$url6"
  wget -q -O $mach.$db.$table "$url"
end

# defensive logic to avoid "sort" disrupting blob tracks
set sort=""
if ($field != "html" && $field != "settings") then
  set sort="1"
  sort $machine1.$db.$table > $machine1.$db.$table$sort 
  sort $machine2.$db.$table > $machine2.$db.$table$sort 
endif

# DO NOT CHANGE this output, unless you want to change all of the following, too:
# compareTrackDbFast.csh
# compareTrackDbAll.csh
# trackDbGlobal.csh

echo
echo "---------------------------------------------------------------"
echo 
# if the line begins with a digit, substitute a newline at the beginning
# | perl -pw -e "s/^(\d)/\nx/"
diff $machine1.$db.$table$sort $machine2.$db.$table$sort | sed -e "/^[0123456789]/s/^/\n/" >& $db.temp
if ( $status ) then
  echo "\n$db.$table.$field : Differences exist between $machine1 and $machine2 \n"
  cat $db.temp
  echo
else
  echo "\n$db.$table.$field : No differences between $machine1 and $machine2 \n"
endif

# clean up
rm -f $machine1.$db.$table 
rm -f $machine2.$db.$table 
rm -f $machine1.$db.$table$sort 
rm -f $machine2.$db.$table$sort
rm -f $db.temp
