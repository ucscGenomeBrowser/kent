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
set rr="false"
set baseUrl=""
set target=""
set hgsid=""
set db=""

if ( $#argv < 2 || $#argv > 3 ) then
  echo
  echo "  checks all links on trackUi pages for a track."
  echo
  echo "    usage:  database tablelist [machine]"
  echo '           tablelist may also be single table or "all"'
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
  checkMachineName.csh $machine
  if ( $status ) then
    exit 1
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
  set tables=`wgetTableField.csh $db trackDb tableName $machine \
     | grep -v tableName`
endif

# set hgsid so don't fill up sessionDb table
set baseUrl="http://$machine.cse.ucsc.edu/"
set hgsid=`htmlCheck  getVars $baseUrl/cgi-bin/hgGateway | grep hgsid \
  | head -1 | awk '{print $4}'`

foreach node (hgw1 hgw2 hgw3 hgw4 hgw5 hgw6 hgw7 hgw8 mgc)
  if ( $machine == $node ) then 
    set rr="true"
  endif
end

foreach table ($tables)
  echo
  echo $table
  echo "============="
  # check to see if the table exists on the machine
  wgetTableField.csh $db trackDb tableName $machine | grep -w $table > /dev/null
  if ( $status ) then
    echo "no such track"
  endif
  set target="$baseUrl/cgi-bin/hgTrackUi?hgsid=$hgsid&db=$db&g=$table"
  htmlCheck checkLinks "$target"
  # slow it down if hitting the RR
  if ( "true" == $rr ) then
    sleep 2
  endif 
end 
echo

