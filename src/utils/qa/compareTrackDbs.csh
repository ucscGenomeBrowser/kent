#!/bin/tcsh
source `which qaConfig.csh`

onintr cleanup

set table = "trackDb"
set field = "tableName"
set cent=""
set host=""

if ($#argv < 3 || $#argv > 4) then
 echo ""
 echo "  compares trackDb on two machines."
 echo "  optionally compares another field instead of tableName."
 echo
 echo "    usage: machine1 machine2 database [field] (defaults to tableName)"
 echo ""
 exit 1
endif

set machine1 = $argv[1]
set machine2 = $argv[2]
set db = $argv[3]

#set machine1 = "hgwdev"
#set machine1 = "hgwbeta"
#set machine2 = "hgw1"

if ( $#argv == 4 ) then
  # check if valid field -- checks on localhost (usually hgwdev)
  set field = $argv[4]
  hgsql -t -e "DESC $table" $db | grep -w $field | wc -l > /dev/null
  if ( $status ) then
    echo
    echo "  $field is not a valid field for $table"
    echo
    exit 1
  endif
endif

# check validity of machine name and existence of db on the machine.
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

  hgsql -N $host -e "SELECT name FROM dbDb" hgcentral$cent \
     | grep -w "$db" >& /dev/null
  if ( $status ) then
    echo
    echo "  database $db is not found in dbDb on $machine"
    echo
    exit 1
  endif
end

foreach mach ( $machine1 $machine2 )
  rm -f $mach.$db.$table 
  getField.csh $db trackDb $field $mach > $mach.$db.out
end

# defensive logic to avoid "sort" disrupting blob tracks
set sort=""
if ($field != "html" && $field != "settings") then
  set sort="1"
  sort $machine1.$db.out > $machine1.$db.out$sort 
  sort $machine2.$db.out > $machine2.$db.out$sort 
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
diff $machine1.$db.out$sort $machine2.$db.out$sort | sed -e "/^[0123456789]/s/^/\n/" >& $db.temp
if ( $status ) then
  echo "\n$db.$table.$field : Differences exist between $machine1 and $machine2 \n"
  cat $db.temp
  echo
else
  echo "\n$db.$table.$field : No differences between $machine1 and $machine2 \n"
endif

cleanup:
rm -f $machine1.$db.out*
rm -f $machine2.$db.out*
rm -f $db.temp
