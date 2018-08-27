#!/bin/tcsh
source `which qaConfig.csh`

onintr cleanup

if ($#argv < 3 || $#argv > 4) then
 echo ""
 echo "  compares trackDb on two machines."
 echo "  optionally compares another field instead of tableName."
 echo "  uses MySQL query for speed, not WGET."
 echo "  uses public mysql machine for RR queries."
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

set table = "trackDb"
set field = "tableName"

if ( $#argv == 4 ) then
  # check if valid field -- checks on dev"
  set field = $argv[4]
  hgsql -t -e "DESC $table" $db | grep -w $field > /dev/null
  if ( $status ) then
    echo
    echo "  $field is not a valid field for $table"
    echo
    echo ${0}:
    $0
    exit 1
  endif
endif

# check individual html longBlobs one at a time and make pretty output
# using list of tracks from beta
# for html and settings fields, using public mysql server 
# -- not RR node-specific
set pubMySqlFlag=0
set tableRow=""
if ( $field == "html" || $field == "settings" ) then
  rm -f $machine1.$db.$table
  rm -f $machine2.$db.$table
  set tracks=`hgsql -N -h $sqlbeta -e "SELECT tableName FROM trackDb" $db`
  foreach row ( $tracks )
    set tableRow=$row
    foreach machX ( $machine1 $machine2 )
      if ( $machX == "hgwbeta" ) then
        hgsql -h $sqlbeta -e 'SELECT '$field' FROM trackDb \
          WHERE tableName = "'$tableRow'"' $db > $machX.$db.$table.$field
      else
        set pubMySqlFlag=1
        mysql --user=genome --host=genome-mysql.soe.ucsc.edu -A \
          -e 'SELECT '$field' FROM trackDb \
          WHERE tableName = "'$tableRow'"' $db > $machX.$db.$table.$field
      endif
      cat $machX.$db.$table.$field | perl -pw -e "s/\\n/\n/g" \
          | sed -e "s/^/${tableRow} \| /" >> $machX.$db.$table
      rm -f $machX.$db.$table.$field
    end
  end

else

  foreach mach ( $machine1 $machine2 )
    rm -f $mach.$db.$table 
    if ( $mach == "hgwbeta" ) then
      hgsql -h $sqlbeta -e 'SELECT '$field' FROM trackDb' $db \
         > $mach.$db.$table
    else
      set pubMySqlFlag=1
      mysql --user=genome --host=genome-mysql.soe.ucsc.edu -A \
        -e "SELECT $field FROM trackDb"  $db > $mach.$db.$table
    endif
  end
endif

# DO NOT CHANGE this output, unless you want to change all of the following, too:
# compareTrackDbs.csh
# compareTrackDbAll.csh
# trackDbGlobal.csh
  
echo
echo "---------------------------------------------------------------"
echo 
# if the line begins with a digit, substitute a newline at the beginning
# | perl -pw -e "s/^(\d)/\nx/"
diff $machine1.$db.$table $machine2.$db.$table | sed -e "/^[0123456789]/s/^/\n/" >& $db.temp
if ( $status ) then
  echo "\n$db.$table.$field : Differences exist between $machine1 and $machine2 \n"
  if ( $pubMySqlFlag == 1 ) then
    echo "(RR fields taken from public MySql server, not individual machine)"
  endif
  cat $db.temp
  echo
else
  echo "\n$db.$table.$field : No differences between $machine1 and $machine2 \n"
endif

cleanup:
rm -f $machine1.$db.$table 
rm -f $machine2.$db.$table 
rm -f $db.temp 
