#!/bin/tcsh


###############################################
#  09-16-04
#  Robert Kuhn
#
#  Checks all fields (except html) in trackDb
###############################################

if ($#argv < 1 || $#argv > 4) then
  echo ""
  echo "  checks all fields in trackDb"
  echo "  this will break when hgText is replaced by hgTables."
  echo
  echo "    usage: database, [machine1], [machine2] (defaults to hgw1 and hgwbeta)"
  echo "                     [mode = terse|verbose for html field] (defaults to terse)"
  echo ""
  exit 1
endif

#set machine1 = "hgwdev"
set machine1 = "hgw1"
set machine2 = "hgwbeta"
set mode="terse"

set db = $argv[1]
if ( $#argv > 2 ) then
  set machine1 = $argv[2]
  set machine2 = $argv[3]
endif

if ( $#argv == 2 ) then
  set mode = $argv[2]
endif

if ( $#argv == 4 ) then
  set mode = $argv[4]
endif

if ( $mode != "verbose" && $mode != "terse" ) then
  echo
  echo "  mode not terse or verbose.\n"
  echo "${0}:"
  $0
  exit 1
endif

checkMachineName.csh $machine1
if ( $status ) then
  echo "${0}:"
  $0
  exit 1
endif

checkMachineName.csh $machine2
if ( $status ) then
  echo "${0}:"
  $0
  exit 1
endif

set table = "trackDb"
set fields=`hgsql -N -e "DESC $table" $db | gawk '{print $1}' | grep -vw "html"`

foreach field ( $fields )
  compareTrackDbs.csh $machine1 $machine2 $db $field
  if ( $status ) then
    exit 1
  endif
end

set field="html"
if ( $mode == "terse" ) then
  set modeDiff=`compareTrackDbs.csh $machine1 $machine2 $db $field | grep "difference"`
  echo $modeDiff | grep "are found" > /dev/null 
  if (! $status ) then
    echo
    echo "  Differences exist in $field field"
    echo
  else
    echo
    echo $modeDiff
    echo
  endif
else
  echo
  compareTrackDbs.csh $machine1 $machine2 $db $field 
endif

exit 0
