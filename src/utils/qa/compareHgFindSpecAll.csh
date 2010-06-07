#!/bin/tcsh
source `which qaConfig.csh`

###############################################
#  Checks all fields in hgFindSpec
###############################################

if ($#argv < 1 || $#argv > 4) then
  echo ""
  echo "  checks all fields in hgFindSpec"
  echo
  echo "    usage: database, [machine1], [machine2] (defaults to hgw1 and hgwbeta)"
  echo "             [mode] (fast) "
  echo ""
  exit 1
endif

#set machine1 = "hgwdev"
set machine1 = "hgw1"
set machine2 = "hgwbeta"
set inputMode=""
set mode2="realTime"

set db = $argv[1]
if ( $#argv > 2 ) then
  set machine1 = $argv[2]
  set machine2 = $argv[3]
endif

if ( $#argv == 2 ) then
  set inputMode = $argv[2]
endif

if ( $#argv == 4 ) then
  set inputMode = $argv[4]
endif

if (! ( $inputMode == "fast" || $inputMode == "realTime" || $inputMode == "" )) then
    echo
    echo "  mode ($inputMode) not acceptable.\n"
    echo "${0}:"
    $0
    exit 1
  endif
endif

if ( $inputMode == "fast" ) then
  set mode2 = "fast" 
endif

foreach mach ( $machine1 $machine2 )
  checkMachineName.csh $mach
  if ( $status ) then
    echo "${0}:"
    $0
    exit 1
  endif
end

set table = "hgFindSpec"
set fields=`hgsql -N -e "DESC $table" $db | gawk '{print $1}' | grep -vw "html"`
if ( $status ) then
  echo  
  echo "  query failed.  check database name."
  echo  
  exit 1
endif


foreach field ( $fields )
  if ( $mode2 == "fast" || $field == "searchSettings" ) then
    compareHgFindSpecFast.csh $machine1 $machine2 $db $field
    if ( $status ) then
      exit 1
    endif
  else
    compareHgFindSpecs.csh $machine1 $machine2 $db $field
    if ( $status ) then
      exit 1
    endif
  endif
end

exit 0
