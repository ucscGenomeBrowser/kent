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
set machine="hgwbeta"
set host="-h hgwbeta"
set found=0
set dbs=""
set rr="false"
set dumpDate=""
set quiet=0

if ($#argv < 1 || $#argv > 3) then
  echo
  echo "  gets the names of all databases that contain a given table."
  echo "  will accept the MySQL wildcard, %, but not on RR machines"
  echo "  note: not real-time on RR.  uses nightly TABLE STATUS dump."
  echo
  echo "    usage:  tablename [machine] [quiet] - defaults to beta"
  echo '              "quiet" suppresses list of assemblies checked'
  echo
  exit
else
  set tablename=$argv[1]
endif

set debug=0

# assign command line arguments

set argNum=$#argv
if ( $argv[$argNum] == "quiet" ) then
  set quiet=1
endif

if (($#argv > 1 && $quiet != 1) || $#argv == 3 ) then
  set machine="$argv[2]"
  set host="-h $argv[2]"
  if ($argv[2] == "hgwdev") then
    set host=""
  endif
else
endif


# check machine validity
if ($debug == 1) then
  echo "tablename = $tablename"
  echo "machine   = $machine"
  echo "host      = $host"
  echo "found     = $found"
  echo "dbs       = $dbs"
  echo "rr        = $rr"
  echo "quiet     = $quiet"
  echo "dumpDate  = $dumpDate"
  echo
endif

checkMachineName.csh $machine

if ( $status ) then
  echo "${0}:"
  $0
  exit 1
endif

# -------------------------------------------------
# get all databases

if ($machine == hgwdev || $machine == hgwbeta) then
  # hgsql -N $host -e "SHOW DATABASES"  proteins > $machine.databases
  set dbs=`hgsql -N $host -e "SHOW DATABASES"`
else
  set host=""
  set dumpDate=`getRRdatabases.csh $machine | grep "last dump" \
     | gawk '{print $NF}'`
  set dbs=`getRRdatabases.csh $machine | grep -v "last dump"`
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
echo "getting all assemblies containing $tablename from $machine"
rm -f $machine.$tablename.foundIn

set chrom=""
set split=""
set isChromInfo=0
if ( $machine == hgwdev || $machine == hgwbeta ) then
  foreach db ( $dbs )
    if ( $quiet == 0 ) then
      echo "checking "$db
    endif
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
    set found=`hgsql -N $host -e 'SHOW TABLES LIKE "'${tablename}'"' $db | wc -l`
    if ( $found > 0 ) then
      echo "$db" >> $machine.$tablename.foundIn
    endif
  end
else   # not dev or beta
  set rr="true"
  echo $tablename | grep "%" > /dev/null
  if (! $status ) then
    echo "sorry, wildcard not supported for RR machines at this time\n"
    exit
  endif
  foreach db ( $dbs )
  # foreach db ( rn3 hg17 )
    if ( $quiet == 0 ) then
      echo "checking "$db
    endif
    # check for chromInfo table on dev 
    #  -- except for one db that is on RR but not dev.
    set isChromInfo=0
    if ( $db != proteins092903 ) then
      hgsql -N $host -e 'SHOW TABLES' $db | grep "chromInfo" > /dev/null
      if ( $status ) then
        set isChromInfo=1
      endif
    endif
    if ( $isChromInfo != 1 ) then
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
  set isChromInfo=""
  end # end foreach db
endif # end if if/else dev, beta
echo 

# report databases found
set ok=""
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

if ( $rr == "true" ) then
  echo "  as of last TABLE STATUS dump, $dumpDate"
  echo 
endif
echo


# cleanup
rm -f $machine.$tablename.foundIn
rm -f $machine.$tablename.split.foundIn
