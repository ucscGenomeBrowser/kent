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
  echo
  echo "    usage: database [machine1] [machine2] [mode] "
  echo
  echo "      (defaults to hgw1 and hgwbeta)"
  echo "      mode = (fast|verbose|fastVerbose) "
  echo              
  echo "               - verbose is for html field - defaults to terse"
  echo "               - fast = (mysql-genome) - defaults to realTime (WGET)"
  echo 
  exit 1
endif

#set machine1 = "hgwdev"
set machine1 = "hgw1"
set machine2 = "hgwbeta"
set inputMode=""
set mode="terse"
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

# if ( $inputMode != "fast" && $inputMode != "fastVerbose" \
#   && $inputMode != "verbose" && $inputMode != "" ) then

if (! ( $inputMode == "fast" || $inputMode == "fastVerbose" \
        || $inputMode == "verbose" || $inputMode == "" )) then
    echo
    echo "  mode ($inputMode) not acceptable.\n"
    echo "${0}:"
    $0
    exit 1
  endif
endif

if ( $inputMode == "verbose" || $inputMode == "fastVerbose" ) then
  set mode = "verbose" 
endif

if ( $inputMode == "fast" || $inputMode == "fastVerbose" ) then
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

set table = "trackDb"
set fields=`hgsql -N -e "DESC $table" $db | gawk '{print $1}' | grep -vw "html"`
if ( $status ) then
  echo  
  echo "  query failed.  check database name."
  echo  
  exit 1
endif


foreach field ( $fields )
  if ( $mode2 == "fast" || $field == "settings" ) then
    # echo "\n mode2 = $mode2"
    # echo "field  = $field"
    compareTrackDbFast.csh $machine1 $machine2 $db $field
    if ( $status ) then
      exit 1
    endif
    echo "  not real time for $field"
  else
    # echo "\n mode2 = $mode2"
    # echo "field  = $field"
    compareTrackDbs.csh $machine1 $machine2 $db $field
    if ( $status ) then
      exit 1
    endif
  endif
end

set field="html"
if ( $mode == "terse" ) then
  # expecting one of these two types of output from compareTrackDbFast.csh:
  # hg17.trackDb.html : Differences exist between hgw1 and hgwbeta
  # hg17.trackDb.html : No differences between hgw1 and hgw2

  set diffLine=`compareTrackDbFast.csh $machine1 $machine2 $db $field \
     | grep -i differences`
  echo $diffLine | grep "exist between $machine1" > /dev/null 
  if ( $status ) then  # errStatus of 1 says there are no diffs
  # DO NOT CHANGE this output, unless you want to change trackDbGlobal parsing, too.
    echo
    echo "  No differences in $field field"
    echo
  else
    echo
    echo "  Differences exist in $field field"
    echo
  endif
else
  # mode is verbose: spew results to stdout 
  echo
  compareTrackDbFast.csh $machine1 $machine2 $db $field 
  echo "  not real time for $field"
  echo
endif

exit 0
