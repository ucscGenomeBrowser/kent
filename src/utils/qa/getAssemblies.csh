#!/bin/tcsh
source `which qaConfig.csh`

#######################
#
#  04-19-04
#  gets the names of all databases that contain a given table
#
#######################

set tablename=""
set machine="hgwbeta"
set host="-h $sqlbeta"
set found=0
set dbs=""
set rr="false"
set dumpDate=""
set quiet=1

set debug=true
set debug=false

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

if ($#argv < 1 || $#argv > 3) then
  echo
  echo "  gets the names of all databases that contain a given table."
  echo "  will accept the MySQL wildcard, %, but not on RR machines"
  echo "  note: not real-time on RR.  uses nightly TABLE STATUS dump."
  echo
  echo "    usage:  tablename [machine] [verbose] - defaults to beta"
  echo '              "verbose" prints list of assemblies checked'
  echo
  exit
else
  set tablename=$argv[1]
endif

# assign command line arguments

if ( $argv[$#argv] == "verbose" ) then
  set quiet=0
endif

if (($#argv > 1 && $quiet == 1) || $#argv == 3 ) then
  set machine="$argv[2]"
  if ($argv[2] == "hgwdev") then
    set host=""
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

if ($machine == hgwdev || $machine == hgwbeta) then
  set dbs=`hgsql -N $host -e "SHOW DATABASES"`
else
  set host=""
  set dumpDate=`getRRdatabases.csh $machine | grep "last dump" \
     | gawk '{print $NF}'`
  set dbs=`getRRdatabases.csh $machine | egrep -v "inform|last dump"`
  echo $dbs | grep "is not a valid" > /dev/null
  if ( $status == 0 ) then
    echo
    exit 1
  endif
endif

if ( $debug == "true" ) then
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

# -------------------------------------------------
# get all assemblies containing $tablename

echo
echo "getting all assemblies containing $tablename from $machine"

set chrom=""
set isSplit=""
set isChromInfo=0
set splitlist=""
set notSplitlist=""
if ( $machine == hgwdev || $machine == hgwbeta ) then
  foreach db ( $dbs )
    if ( $quiet == 0 ) then
      echo "checking "$db
    endif
    # check for chromInfo table
    set isChromInfo=`hgsql -N $host -e 'SHOW TABLES' $db | grep "chromInfo" \
       | wc -l`

    # check if split table
    if ( $isChromInfo > 0 ) then
      set chrom=`hgsql -N $host -e 'SELECT chrom FROM chromInfo LIMIT 1' $db`
      set isSplit=`hgsql -N $host -e 'SHOW TABLES LIKE "'${chrom}_$tablename'"' \
        $db | wc -l`
      # echo "  split = $isSplit"
      if ( $isSplit != 0 ) then
        set splitlist=`echo "$splitlist $db"`
      endif
    endif
    # if no chromInfo or if table not split
    set found=`hgsql -N $host -e 'SHOW TABLES LIKE "'$tablename'"' $db | wc -l`
    if ( $found > 0 ) then
      set notSplitlist=`echo "$notSplitlist $db"`
    endif
  end
else   # not dev or beta
  set rr="true"
  echo $tablename | grep "%" > /dev/null
  if (! $status ) then
    echo "sorry, wildcard not supported for RR machines at this time\n"
    exit 1
  endif
  foreach db ( $dbs )
    if ( $quiet == 0 ) then
      echo "checking "$db
    endif
    # check for chromInfo table on dev 
    hgsql -N $host -e 'SHOW TABLES' $db | grep "chromInfo" > /dev/null
    if ( $status ) then
      set isChromInfo=0
    else
      set isChromInfo=1
    endif
    if ( $isChromInfo == 1 ) then
      set chrom=`hgsql -N $host -e 'SELECT chrom FROM chromInfo LIMIT 1' $db` 
      # check if split table (on dev)
      set isSplit=`hgsql -N $host -e 'SHOW TABLES LIKE "'${chrom}_$tablename'"' \
        $db | wc -l`
      if ( $isSplit == 1 ) then
        set found=`getRRtables.csh $machine $db | grep -w "${chrom}_$tablename" \
          | wc -l`
        if ( $found == 1 ) then
          set splitlist=`echo "$splitlist $db"`
        endif
      endif
    endif
    # if no chromInfo or if table not split
    set found=`getRRtables.csh $machine $db | grep -w "$tablename" \
      | wc -l`
    if ( $found == 1 ) then
      set notSplitlist=`echo "$notSplitlist $db"`
    endif
    set isChromInfo=""
  end # end foreach db
endif # end if if/else dev, beta
echo 

# report databases found
set ok=""

#check for wildcard and print tables, too, if so
echo $tablename | grep -v % > /dev/null
set isWildcard=$status
if ( $isWildcard == 0 ) then
  if ( `echo $notSplitlist | awk '{print $1}'` != "" ) then
    set ok=1
    echo "$tablename found in:\n"
    echo $notSplitlist | sed -e "s/ /\n/g"
    echo
  endif
  if ( `echo $splitlist | awk '{print $1}'` != "" ) then
    set ok=1
    echo "split_$tablename found in:\n"
    echo $splitlist | sed -e "s/ /\n/g"
  endif
else
  # get tablenames for wildcards
  # unsplit first
  if ( `echo $notSplitlist | awk '{print $1}'` != "" ) then
    set ok=1
    echo "$tablename found in:\n"
    foreach db ( $notSplitlist )
      set list=`hgsql -N $host -e 'SHOW TABLES LIKE "'$tablename'"' $db`
      echo $db
        foreach table ( $list )
          echo $table | awk '{printf("   %-65s\n", $1)}'
        end
      echo
    end
    echo
  endif

  # split 
  if ( `echo $splitlist | awk '{print $1}'` != "" ) then
    set ok=1
    echo "split_$tablename found in:\n"
    foreach db ( $splitlist )
      set chrom=`hgsql -N $host -e 'SELECT chrom FROM chromInfo LIMIT 1' $db`
      set list=`hgsql -N $host -e 'SHOW TABLES LIKE "'${chrom}_$tablename'"' $db`
      echo $db
        foreach table ( $list )
          echo $table | awk '{printf("   %-65s\n", $1)}'
        end
      echo
    end
    echo
  endif
endif

if ( $ok != 1 ) then
  echo "neither $tablename nor split_$tablename are found on $machine"
endif

if ( $rr == "true" ) then
  echo "  as of last TABLE STATUS dump, $dumpDate"
  echo 
endif
echo
