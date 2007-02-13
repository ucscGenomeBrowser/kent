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
set dbs=""
set url1=""
set url2=""
set list="false"
set size=""
set totSize=0
set mach="hgwbeta"

if ( $#argv != 1 ) then
  echo
  echo "  gets size of database from TABLE STATUS dumps"
  echo "  form hgwbeta only"
  echo
  echo "    usage:  database | all | filename"
  echo "           defaults to hgwbeta"
  echo "           not real time"
  echo
  exit
else
  set db=$argv[1]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
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
  endif
endif

echo "db" "Gbytes" | awk '{printf ("%7s %5s\n", $1, $2)}'
echo "======= =====" 
foreach database ( `echo $db | sed -e "s/ /\n/"` )
  set url1="http://$mach.cse.ucsc.edu/cgi-bin/hgTables"
  set url2="?hgta_doMetaData=1&db=$database&hgta_status=1"
  set size=`wget -q -O /dev/stdout "$url1$url2" \
    | awk '{total+=$6} END {printf "%0.1f", total/1000000000}'`
  echo $database $size | awk '{printf ("%7s %5s\n", $1, $2)}'
  set totSize=`echo $totSize $size | awk '{print $1+$2}'`
end

if ( "true" == $list ) then
  echo "\n total = $totSize Gbytes \n"
endif
