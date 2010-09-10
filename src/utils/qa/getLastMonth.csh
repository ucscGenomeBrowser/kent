#!/bin/tcsh
source `which qaConfig.csh`

#######################
#
#  10-08-07
#  gets year and month of last month 
#  
#  Robert Kuhn
#
#######################

set year=""
set month=""
set yearMonth=""
set lastMonth=""

if ( $#argv != 1 ) then
  echo
  echo "  gets year and month of last/prev month."
  echo
  echo "    usage: go | YYYY-MM"
  echo
  exit 1
else
  if ( $argv[1] == "go" ) then
    set year=`date +%Y`
    set month=`date +%m`
    set yearMonth=`date +%Y-%m`
  else
    set yearMonth=$argv[1]
    set year=`echo $yearMonth | awk -F- '{print $1}'`
    set month=`echo $yearMonth | awk -F- '{print $2}'`
  endif
endif

# for some reason, 08 and 09 make the if statement barf, so drop zero
set month=`echo $month | sed "s/^0//"`
#  echo "month = $month"

if ( $month > 12 || $month < 1 ) then
  echo "\n error.  month out of range.\n"
  exit 1
endif

# echo "year = $year"
# echo "month = $month"
# echo "yearMonth = $yearMonth"

# find last month
set lastMonth=`echo $month | awk '{print $1-1}'`
if ( $lastMonth == 0 ) then
  set lastMonth=12
  set year=`echo $year | awk '{print $1-1}'`
endif

if ( $lastMonth < 10 ) then
  set lastMonth='0'$lastMonth
endif

# echo "year = $year"
# echo "lastMonth = $lastMonth"

echo $year-$lastMonth

exit

