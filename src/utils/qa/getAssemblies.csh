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
  echo "  note: not real-time on RR.  uses nightly TABLE STATUS dump."
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
    set host="-h $argv[2]"
    set machine="$argv[2]"
  endif
else
endif

# check machine validity

checkMachineName.csh $machine

if ( $status ) then
  echo "${0}:"
  $0
  exit 1
endif

# -------------------------------------------------
# get all databases

echo 
if ($machine == hgwdev || $machine == hgwbeta) then
  # hgsql -N $host -e "SHOW DATABASES"  proteins > $machine.databases
  set dbs=`hgsql -N $host -e "SHOW DATABASES"  proteins`
else
  set host=""
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
echo "get all assemblies containing $tablename from $machine"
rm -f $machine.$tablename.foundIn
echo 

set chrom=""
set split=""
set isChromInfo=""
if ( $machine == hgwdev || $machine == hgwbeta ) then
  foreach db ( $dbs )
    echo "checking "$db
    # check for chromInfo table
    set isChromInfo=`hgsql -N $host -e 'SHOW TABLES' $db | grep "chromInfo" \
       | wc -l`
    if ( $isChromInfo > 0 ) then
      set chrom=`hgsql -N $host -e 'SELECT chrom FROM chromInfo LIMIT 1' $db`

      # check if split table
      set split=`hgsql -N $host -e 'SHOW TABLES LIKE "'${chrom}_$tablename'"' \
        $db | wc -l`
      # echo "  split = $split"
      if ( $split == 1 ) then
        # echo "$db \t$found\t split = $split" >> $machine.$tablename.foundIn
        echo "$db" >> $machine.$tablename.split.foundIn
        continue
      endif
    endif
    # seek if not split table
    set found=`hgsql -N $host -e 'SHOW TABLES LIKE "'$tablename'"' $db | wc -l`
    if ( $found == 1 ) then
      echo "$db" >> $machine.$tablename.foundIn
    endif
  end
else   # not dev or beta
  foreach db ( $dbs )
  # foreach db ( rn3 hg17 )
    echo "checking "$db
    # check for chromInfo table on dev 
    #  -- except for one db that is on RR but not dev.
    if ( $db != proteins092903 ) then
      set isChromInfo=`hgsql -N $host -e 'SHOW TABLES' $db | grep "chromInfo"`
    endif
    if ( $isChromInfo != "" ) then
      set chrom=`hgsql -N $host -e 'SELECT chrom FROM chromInfo LIMIT 1' $db`
      # check if split table (on dev)
      set split=`hgsql -N $host -e 'SHOW TABLES LIKE "'${chrom}_$tablename'"' \
        $db | wc -l`
      if ( $split == 1 ) then
        set found=`getRRtables.csh $machine $db | grep -w "${chrom}_$tablename" \
          | wc -l`
        if ( $found == 1 ) then
          echo "$db" >> $machine.$tablename.split.foundIn
          continue
        endif
      endif
    endif
    # if no chromInfo or if table not split
    set found=`getRRtables.csh $machine $db | grep -w "$tablename" \
      | wc -l`
    if ( $found == 1 ) then
      echo "$db" >> $machine.$tablename.foundIn
    endif
  end # end foreach db
endif # end if if/else dev, beta
echo 

# report databases found
set ok=""
echo
if ( -e $machine.$tablename.foundIn ) then
  echo "$tablename found in:\n"
  cat $machine.$tablename.foundIn
  echo
  set ok=1
endif
if ( -e $machine.$tablename.split.foundIn ) then
  echo "split_$tablename found in:\n"
  cat $machine.$tablename.split.foundIn
  set ok=1
endif
if ( $ok != 1 ) then
  echo "neither $tablename nor split_$tablename are found on $machine"
endif
echo

# cleanup
rm -f $machine.$tablename.foundIn
rm -f $machine.$tablename.split.foundIn
