#!/bin/tcsh

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
set url1="http://$mach.cse.ucsc.edu/cgi-bin/hgTables"
set url2=""
set list="false"
set size=0
set totSize=0
set indexSize=0
set totIndexSize=0

if ( $#argv < 1 || $#argv > 2 ) then
  echo
  echo "  gets size of database from TABLE STATUS dumps"
  echo "    from hgwbeta only."
  echo "  opitonally suppresses size of indices."
  echo
  echo "    usage:  database|all|filename [noIndex]"
  echo "         defaults to hgwbeta"
  echo '         "filename" refers to list of dbs'
  echo
  exit
else
  set db=$argv[1]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
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
    set db=`hgsql -h hgwbeta -N -e "SELECT name FROM dbDb \
     WHERE active != 0" hgcentralbeta | sed -e "s/\t/\n/" | sort`
  else
    set url2="?hgta_doMetaData=1&hgta_metaDatabases=1"
    wget -q -O /dev/stdout "$url1$url2" | grep -w $db >& /dev/null
    if ( $status ) then
      echo
      echo " $db does not exist on $mach" 
      echo
      exit 1
    endif
  endif
endif

# print headers
echo
if ( "index" == $index ) then
  echo "db" "Gb" "index" | awk '{printf ("%7s %6s %5s\n", $1, $2, $3)}'
  echo "======= ====== =====" 
else
  echo "db" "Gb" | awk '{printf ("%7s %6s\n", $1, $2)}'
  echo "======= ======" 
endif

# do data
foreach database ( `echo $db | sed -e "s/ /\n/"` )
  set url2="?hgta_doMetaData=1&db=$database&hgta_metaStatus=1"
  set size=`wget -q -O /dev/stdout "$url1$url2" \
    | awk '{total+=$6} END {printf "%0.1f", total/1000000000}'`
  set totSize=`echo $totSize $size | awk '{print $1+$2}'`
  if ( $index == "index" ) then
    set indexSize=`wget -q -O /dev/stdout "$url1$url2" \
      | awk '{total+=$8} END {printf "%0.1f", total/1000000000}'`
    echo $database $size $indexSize \
      | awk '{printf ("%7s %6s %5s\n", $1, $2, $3)}'
    set totIndexSize=`echo $totIndexSize $indexSize | awk '{print $1+$2}'`
  else
    echo $database $size | awk '{printf ("%7s %6s\n", $1, $2)}'
  endif
end

# print totals
if ( "true" == $list ) then
  if ( "index" == $index ) then
    echo
    echo "total" "=" $totSize $totIndexSize \
      | awk '{printf ("%5s %1s %6s %5s\n", $1, $2, $3, $4)}'

  else
    echo "\n total = $totSize Gb "
  endif
endif
#print grand total if index

if ( "index" == $index ) then
  echo
  echo "combined $totSize  $totIndexSize Gb" | awk '{print $1, $2+$3, $4}'
endif

echo
