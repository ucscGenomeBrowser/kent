#!/bin/tcsh

#######################
#
#  04-19-04
#  gets the names of all databases that contain a given table
#  logs into proteins to get database list
#    (proteins should always be there)
#
#######################

set tablename=""
set machine="hgwdev"
set host=""
set found=0
set dbs=""

if ($#argv < 1 || $#argv > 2) then
  echo
  echo "  gets the names of all databases that contain a given table."
  echo
  echo "    usage:  tablename, [machine] defaults to dev"
  echo
  exit
else
  set tablename=$argv[1]
endif

# assign command line arguments
if ($#argv == 2) then
  if ($argv[2] != "hgwdev") then
    set host="-h $2"
    set machine="$2"
  endif
else
endif

# check machine validity
set validMach1=`echo $machine | grep "hgw" | wc -l`
set validMach2=`echo $machine | grep "mgc" | wc -l`
if ($validMach1 == 0 && $validMach2 == 0) then
  echo
  echo "   $machine is not a valid machine."
  echo "   usage:  tablename, [machine] defaults to dev"
  echo
  exit 1
endif

# -------------------------------------------------
# get all databases

echo 
if ($machine == hgwdev || $machine == hgwbeta) then
  # hgsql -N $host -e "SHOW DATABASES"  proteins > $machine.databases
  set dbs=`hgsql -N $host -e "SHOW DATABASES"  proteins`
else
  set dbs=`getRRdatabases.csh $machine`
  set checkMach=`echo $dbs | grep "is not a valid"`
  if ( $status == 0 ) then
    echo "  $dbs"
    echo
    exit
  endif
endif

# -------------------------------------------------
# get all assemblies containing $tablename

echo
echo "get all assemblies containing $tablename"
rm -f $machine.$tablename.foundIn
echo 

if ($machine == hgwdev || $machine == hgwbeta) then
  foreach db ($dbs)
    echo "checking "$db
    set found=`hgsql -N $host -e 'SHOW TABLES LIKE "'$tablename'"' $db | wc -l`
    if ($found == 1) then
       echo $db >> $machine.$tablename.foundIn
    endif
  end
else
  foreach db ($dbs)
    echo "checking "$db
    set found=`getRRtables.csh $machine $db | grep ^"$tablename"$ | wc -l`
    if ($found == 1) then
       echo $db >> $machine.$tablename.foundIn
    endif

  end

endif
echo 

# report databases found
echo
if (-e $machine.$tablename.foundIn) then
  echo "$tablename found in:"
  cat $machine.$tablename.foundIn
else
  echo "$tablename not found on $machine"
endif
echo

# cleanup
rm -f $machine.$tablename.foundIn
