#!/bin/tcsh

#######################
#
#  10-30-06
#  get the assemblies that use a particular db in the Conservation track.
#
#  Robert Kuhn and Brooke Rhead
#
#######################

set tablename=""
set machine="hgwbeta"
set host="-h hgwbeta"
set db=""
set dbs=""
set all=0

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

if ( "$HOST" != "hgwbeta" && "$HOST" != "hgwdev"  ) then
 echo "\n error: only hgwbeta and hgwdev are available\n"
 exit 1
endif

if ($#argv < 1 || $#argv > 2 ) then
  echo
  echo "  get the assemblies that use a particular db in the Conservation track."
  echo
  echo "    usage:  db [machine] OR all [machine] - defaults to beta"
  echo '             where "all" prints species list for all tracks in all dbs.'
  echo "             does not work on RR machines."
  echo
  exit
endif

set debug=0

# assign command line arguments

if ($argv[1] == "all" ) then
  set all=1
else
  set db=$argv[1]
endif

if ($#argv == 2 ) then
  set machine="$argv[2]"
  set host="-h $argv[2]"
  if ($argv[2] == "hgwdev") then
    set host=""
  endif
endif

if ($machine != "hgwdev" && $machine != "hgwbeta" ) then
  echo "\n  Sorry, only works for hgwdev and hgwbeta \n"
  exit 1
endif

# check machine validity
checkMachineName.csh $machine
if ( $status ) then
  echo "${0}:"
  $0
  exit 1
endif

if ($debug == 1) then
  echo "tablename = $tablename"
  echo "machine   = $machine"
  echo "host      = $host"
  echo "db        = $db"
  echo "rr        = $rr"
  echo "dumpDate  = $dumpDate"
  echo
endif

# -------------------------------------------------
# get all assemblies containing multiz%

set dbs=`getAssemblies.csh multiz% $machine | grep -v "multiz" | egrep "."`

echo
foreach assembly ( $dbs )
  set multilist=`hgsql $host -N -e "SELECT tableName FROM trackDb \
     WHERE tableName LIKE 'multiz%'" $assembly` 
  if ( $all == 1) then
    foreach table ( $multilist )
      echo $assembly $table
      echo "-------"
      hgsql $host -N -e 'SELECT settings FROM trackDb \
         WHERE tableName = "'$table'"' $assembly | sed -e "s/\\n/\n/g" \
         | egrep "^sGroup|speciesOrder" | sed -e "s/ /\n/g" \
         | egrep -v "^sGroup|speciesOrder"
      echo "=========================="
    end
  else
    foreach table ( $multilist )
      hgsql $host -N -e 'SELECT settings FROM trackDb \
         WHERE tableName = "'$table'"' $assembly | sed -e "s/\\n/\n/g" \
         | egrep "^sGroup|speciesOrder" | sed -e "s/ /\n/g" \
         | egrep -v "^sGroup|speciesOrder" | grep $db > /dev/null
      if (! $status ) then
        # echo "$assembly $table" | egrep "." | awk '{printf("%-7s %-15s", $1, $2)}'
        echo "$assembly $table" | awk '{printf("%-7s  %-25s\n", $1, $2)}'
      endif
    end
  endif
end
echo
exit


