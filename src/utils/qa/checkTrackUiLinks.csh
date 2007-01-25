#!/bin/tcsh

################################
#  
#  01-24-07
#  Robert Kuhn
#
#  checks all links on trackUi pages for a track
#
################################

set tableinput=""
set tables=""
set machine="hgwbeta"
set baseUrl=""
set target=""
set hgsid=""
set db=""

if ( $#argv < 2 || $#argv > 3 ) then
  echo
  echo "  checks all links on trackUi pages for a track."
  echo
  echo "    usage:  database tablelist [machine]"
  echo '           tablelist may be single table or "all"'
  echo "           machine defaults to hgwbeta"
  echo
  exit
else
  set db=$argv[1]
  set tableinput=$argv[2]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

# set machine name and check validity
if ( $#argv == 3 ) then
  set machine="$argv[3]"
  if ( "genome" != $machine ) then
    checkMachineName.csh $machine
    if ( $status ) then
      exit 1
    endif
  endif
endif

# check if it is a file or a tablename and set list
file $tableinput | egrep "ASCII text" > /dev/null
if (! $status) then
  set tables=`cat $tableinput`
else
  set tables=$tableinput
endif

# process "all" choice
if ("all" == $tableinput) then
  set tables=`ssh -x qateam@hgw1 mysql -A -N \
    -e '"'SELECT tableName FROM trackDb'"' $db`
endif

# set hgsid so don't fill up sessionDb table
if ( "genome" == $machine ) then
  set baseUrl="http://$machine.ucsc.edu/"
else
  set baseUrl="http://$machine.cse.ucsc.edu/"
endif
set hgsid=`htmlCheck  getVars $baseUrl/cgi-bin/hgGateway | grep hgsid \
  | head -1 | awk '{print $4}'`

foreach table ($tables)
  echo
  echo $table
  echo "============="
  # check to see if the table exists on the machine
  ssh -x qateam@hgw1 mysql -A -N -e '"'SELECT tableName FROM trackDb'"' $db \
    | grep $table > /dev/null
  if ( $status ) then
    echo "no such track"
  endif
  set target="$baseUrl/cgi-bin/hgTrackUi?hgsid=$hgsid&db=$db&g=$table"
  htmlCheck checkLinks "$target"
end 
echo

