#!/bin/tcsh


###############################################
#  09-16-04
#  Robert Kuhn
#
#  Checks all fields (except html) in trackDb
###############################################

if ($#argv < 1 || $#argv > 4) then
 echo ""
 echo "  checks all fields (except html) in trackDb"
 echo "  this will break when hgText is removed."
 echo
 echo "    usage: database, machine1, machine2"
 echo ""
 exit 1
endif

#set machine1 = "hgwdev"
#set machine1 = "hgwbeta"
#set machine2 = "hgw1"

set db = $1
set machine1 = $2
set machine2 = $3

set validMach1=`echo $machine1 | grep "hgw" | wc -l`
set validMach2=`echo $machine2 | grep "hgw" | wc -l`

if ($validMach1 == 0 || $validMach2 == 0) then
  echo
  echo "    These are not valid machine names: $machine1 $machine2"
  echo "    usage: database, machine1, machine2"
  echo
  exit 1
endif

set table = "trackDb"

set fields=`hgsql -N -e "DESC $table" $db | gawk '{print $1}' | grep -v "html"`

foreach field ($fields)
  compareTrackDbs.csh $machine1 $machine2 $db $field
end

exit
