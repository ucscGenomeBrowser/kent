#!/bin/tcsh

################################
#
#  06-16-08
#  Ann Zweig
#
#  gets information about which blatServer hosts which genome(s)
#
################################

set db=''
set order=''
set limit=''

if ( $#argv < 1 || $#argv > 2 ) then
  echo
  echo " gets info about which blat server hosts which genome(s)"
  echo
  echo " usage:  db | all  [db | host]"
  echo "   first parameter is required: one specific db or all databases"
  echo "   second parameter is optional: order by db or by host (blatServer)"
  echo
  exit
else
  set db=$argv[1]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n ERROR: you must run this script on dev!\n"
 exit 1
endif

if ( $#argv == 2 ) then
 set order=$argv[2]
endif

if ( all == "$db" ) then
 set db='%'
else
 set limit="LIMIT 1"
endif

hgsql -h genome-centdb hgcentral -e "SELECT db, host FROM blatServers WHERE db LIKE '$db' ORDER BY '$order' $limit"

