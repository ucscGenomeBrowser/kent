#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  02-13-07
#  Robert Kuhn
#
#  gets size of a database from TABLE STATUS dumps
#
################################

set db=""
set index="index"
set mach="hgwbeta"
set url1="http://$mach.soe.ucsc.edu/cgi-bin/hgTables"
set url2=""
set list="false"
set size=0
set totSize=0
set gbdb=0
set totGbdb=0
set indexSize=0
set totIndexSize=0

if ( $#argv < 1 || $#argv > 1 ) then
  echo
  echo "  gets size of database and /gbdb files from TABLE STATUS"
  echo "    from beta only."
#  echo "  optionally suppresses size of indices."
  echo
#  echo "    usage:  database|all|filename [noIndex]"
  echo "    usage:  database|all|filename"
  echo '         "filename" refers to list of dbs'
  echo
  exit
else
  set db=$argv[1]
endif

if ( $#argv == 2 ) then
  set index=$argv[2]
  if ( "noIndex" != $index ) then
    echo
    echo ' error.  second argument must be "noIndex" ' 
    $0
    echo
    exit
  endif
endif

# check for file of db names
file $db | egrep "ASCII text" > /dev/null
if ( ! $status ) then
  set db=`cat $db`
  set list="true"
else
  if ( $db == "all" ) then
    set list="true"
    set db=`hgsql -h $sqlbeta -N -e "SELECT name FROM dbDb \
     WHERE active != 0" hgcentralbeta | sed -e "s/\t/\n/" | sort`
  else
    set url2="?hgta_doMetaData=1&hgta_metaDatabases=1"
    # wget -q -O /dev/stdout "$url1$url2" | grep -w $db >& /dev/null
    wget -q -O /dev/stdout "$url1$url2" | grep -w $db 
    if ( $status ) then
      echo
      echo " $db does not exist on $mach" 
      echo
      exit 1
    endif
  endif
endif

# echo $db
echo
echo "hgwbeta"
echo

# print headers
if ( "index" == $index ) then
  echo "db" "tables" "index" "files" | awk '{printf ("%7s %6s %5s %6s\n", $1, $2, $3, $4)}'
  echo "======= ====== ===== ======" 
else
  echo "db" "tables" | awk '{printf ("%7s %6s %6s\n", $1, $2, $3)}'
  echo "======= ====== ======" 
endif

# do data
foreach database ( `echo $db | sed -e "s/ /\n/"` )
  set url2="?hgta_doMetaData=1&db=$database&hgta_metaStatus=1"
  set size=`wget -q -O /dev/stdout "$url1$url2" \
    | awk '{total+=$7} END {printf "%0.1f", total/1000000000}'`
  set totSize=`echo $totSize $size | awk '{print $1+$2}'`
  # set gbdb=`ssh hgwbeta "du -hc /gbdb/$database/*" | tail -1 | sed 's/G//'`
  set gbdb=`ssh hgwbeta "du -c /gbdb/$database/*" | tail -1 \
      | awk '{printf ("%3.1f\n", $1/1000000)}'`  # du reports in kbytes
  set totGbdb=`echo $totGbdb $gbdb | awk '{print $1+$2}'`

  if ( 1 == 0 ) then  #debug
    echo "url2 = $url2"
    echo "size = $size"
    echo "totSize = $totSize"
  endif

  if ( $index == "index" ) then
    set indexSize=`wget -q -O /dev/stdout "$url1$url2" \
      | awk '{total+=$9} END {printf "%0.1f", total/1000000000}'`
    echo $database $size $indexSize $gbdb\
      | awk '{printf ("%7s %6s %5s %6s\n", $1, $2, $3, $4)}'
    set totIndexSize=`echo $totIndexSize $indexSize | awk '{print $1+$2}'`
  else
    echo $database $size $files | awk '{printf ("%7s %6 %5ss\n", $1, $2, $3)}'
  endif
end

# print totals
if ( "true" == $list ) then
  if ( "index" == $index ) then
    echo "------- ------ ----- -----" 
    echo "total" $totSize $totIndexSize $totGbdb "Gb"\
      | awk '{printf ("%7s %6s %5s %5s %3s\n", $1, $2, $3, $4, $5)}'
  else
    echo "------- ------ -----" 
    echo "\n total tables = $totSize Gb "
  endif
endif

#print grand total if index
if ( "index" == $index ) then
  echo
  echo "combined tables $totSize  $totIndexSize Gb" \
      | awk '{printf ("%8s %6s %5.1f %4s\n", $1, $2, $3+$4, $5)}'
endif

echo
