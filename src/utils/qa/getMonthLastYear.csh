#!/bin/tcsh
source `which qaConfig.csh`

#######################
#
#  09-17-07
#  gets year and month of month last year
#  
#  Robert Kuhn
#
#######################

set go=""
set year=""
set yearMonth=""
set lastyear=""

if ( $#argv != 1 ) then
  echo
  echo "  gets year and month of month last year"
  echo
  echo "    usage:  go"
  echo
  exit 1
else
  if ( $argv[1] != "go" ) then
    echo
    echo '  argument can only be "go"'
    $0
    exit 1
  endif
endif


# find same month last year
set year=`date +%Y`
set yearMonth=`date +%Y-%m`
set lastyear=`echo $year | awk '{print $1-1}'`
set lastYearMonth=`echo $yearMonth | sed -e "s/$year/$lastyear/"`


# echo "year      $year"
# echo "lastyear  $lastyear"
# echo "yearMonth $yearMonth"

echo $lastYearMonth

exit

