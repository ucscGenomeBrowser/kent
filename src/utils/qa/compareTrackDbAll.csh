#!/bin/tcsh


###############################################
#  09-16-04
#  Robert Kuhn
#
#  Checks all fields (except html) in trackDb
###############################################

if ($#argv < 1 || $#argv > 3) then
 echo ""
 echo "  checks all fields in trackDb"
 echo "  this will break when hgText is replaced by hgTables."
 echo
 echo "    usage: database, [machine1], [machine2] (defaults to hgw1 and hgwbeta)"
 echo ""
 exit 1
endif

#set machine1 = "hgwdev"
set machine1 = "hgw1"
set machine2 = "hgwbeta"

set db = $argv[1]
if ( $#argv == 3 ) then
  set machine1 = $argv[2]
  set machine2 = $argv[3]
endif

checkMachineName.csh $machine1
if ( $status ) then
  echo "    usage: database, [machine1], [machine2] (defaults to hgw1 and hgwbeta)"
  echo
  exit 1
endif

checkMachineName.csh $machine2
if ( $status ) then
  echo "    usage: database, [machine1], [machine2] (defaults to hgw1 and hgwbeta)"
  echo
  exit 1
endif

set table = "trackDb"
set fields=`hgsql -N -e "DESC $table" $db | gawk '{print $1}'`

foreach field ( $fields )
  compareTrackDbs.csh $machine1 $machine2 $db $field
end

exit
