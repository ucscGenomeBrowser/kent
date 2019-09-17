#!/bin/tcsh
source `which qaConfig.csh`

if ($#argv < 3 || $#argv > 4) then
 echo ""
 echo "  compares hgFindSpec on two machines."
 echo "  optionally compares another field instead of searchName."
 echo "  uses MySQL query for speed, not WGET."
 echo "  uses public mysql machine for RR queries."
 echo
 echo "    usage: machine1 machine2 database [field] (defaults to searchName)"
 echo ""
 exit 
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

set table = "hgFindSpec"
set field = "searchName"

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
if ( $field == "searchSettings" ) then
  rm -f $machine1.$db.$table
  rm -f $machine2.$db.$table
  set specs=`hgsql -N -h $sqlbeta -e "SELECT searchName FROM hgFindSpec" $db`
  foreach row ( $specs )
    set tableRow=$row
    foreach machX ( $machine1 $machine2 )
      if ( $machX == "hgwbeta" ) then
        hgsql -h $sqlbeta -e 'SELECT '$field' FROM hgFindSpec \
          WHERE searchName = "'$tableRow'"' $db > $machX.$db.$table.$field
      else
        set pubMySqlFlag=1
        mysql --user=genome --host=genome-mysql.soe.ucsc.edu -A \
          -e 'SELECT '$field' FROM hgFindSpec \
          WHERE searchName = "'$tableRow'"' $db > $machX.$db.$table.$field
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
      hgsql -h $sqlbeta -e 'SELECT '$field' FROM hgFindSpec' $db \
         > $mach.$db.$table
    else
      set pubMySqlFlag=1
      mysql --user=genome --host=genome-mysql.soe.ucsc.edu -A \
        -e 'SELECT '$field' FROM hgFindSpec'  $db > $mach.$db.$table
    endif
  end
endif
  
diff $machine1.$db.$table $machine2.$db.$table \
   | sed -e "/^[0123456789]/s/^/\n/"
   # if the line begins with a digit, substitute a newline at the beginning
   # | perl -pw -e "s/^(\d)/\nx/"
if ( $status ) then
  echo "\nThe differences above are found in $table.$field"
  echo "between $machine1 and $machine2"
  if ( $pubMySqlFlag == 1 ) then
    echo "(RR fields taken from public MySql server, not individual machine)"
  endif
  echo
else
  echo "\n  No differences in $db.$table.$field \n  between $machine1 and $machine2 "
  echo
endif

# clean up
rm -f $machine1.$db.$table 
rm -f $machine2.$db.$table 
