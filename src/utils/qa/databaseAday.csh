#!/bin/tcsh

#########################
# 
#  01-11-07
#  Ann Zweig
#
#  Scrolls through all of the databases on the RR
#  one per day.  Monitoring scripts can call this
#  script to find out the "database of the day".
#
########################## 

set choice=''
set activeDbs=''
set todaysDb=''
set error="ERROR - no database list. Try this first: databaseAday.csh init"

if ( $#argv != 1 ) then
  echo
  echo " call with 'today' to find out the database we are monitoring today"
  echo
  echo "    usage:  today | init | next"
  echo
  exit 1
else
  set choice="$argv[1]"
endif

if ( $choice != "today" && $choice != "init" && $choice != "next") then
  echo
  echo " you must call this script using either 'today', 'init' or 'next'"
  echo
  exit 1
endif

# run only from hgwdev
if ( "$HOST" != "hgwdev" ) then
  echo
  echo "ERROR: you must run this script on hgwdev!\n"
  exit 1
endif

# this is a 'today' call, just get the top db on the list and exit
if ( $choice == "today" ) then
  # make sure the databaseAdayList exists
  if (! -e databaseAdayList) then
    echo "$error\n"
    exit 1
  endif
  # return the database at the top of the list
  set todaysDb=`cat databaseAdayList | head -1`
  # be sure we haven't completely wrapped around the list
  if ( $todaysDb == "DO-OVER" ) then
    echo "$error\n"
    exit 1
  endif
  # all is good!
  echo $todaysDb
  exit 0
endif

# this is an initialization call -- query the RR to get the 
# list of active databases (replace the previous list (if any) with this new one)
if ( $choice == "init" ) then
  # remove existing files
  if (-e XXnotActiveXX) then
    rm XXnotActiveXX
  endif
  if (-e databaseAdayList) then
    rm databaseAdayList
  endif

  # get list of active=0 (exclude these from the list)
  hgsql -h genome-centdb hgcentral -B -N \
    -e "SELECT name FROM dbDb WHERE active = 0" > XXnotActiveXX

  # get list of active $gbd databases
  set activeDbs=`ssh -x qateam@hgw1 mysql hg18 -A -N \
    -e '"'SHOW DATABASES'"' \
    | egrep -v "sp0|lost|prote|uni|visi|test|mysql|go|hgFixed" \
    | egrep -vf XXnotActiveXX`

  # put the list (which is now on one line) into separate lines
  echo $activeDbs | sed -e "s/ /\n/g" > databaseAdayList

  # add a tag at the end of the list (this will show when it's time to create a new list)
  echo "DO-OVER" >> databaseAdayList 

  # give the file the correct permissions
  chmod 774 databaseAdayList

  # clean up files
  rm XXnotActiveXX
  exit 0
endif

# this is a next call -- edit the list so that the next database is at the top
# this call happens automatically each night from a qateam cronjob
if ( $choice == "next" ) then
  # make sure the list exists
  if (! -e databaseAdayList) then
    echo "$error\n"
    exit 1
  endif
  
  # get the database at the top of the list
  set todaysDb=`cat databaseAdayList | head -1`
  # be sure we haven't completely wrapped around the list
  # if so, reinitialize list and quit
  if ( $todaysDb == "DO-OVER" ) then
    $0 init
    exit 0
  endif
  # put the top db at the bottom of the list
  grep -v $todaysDb databaseAdayList > XXdatabaseAdayTempListXX
  echo $todaysDb >> XXdatabaseAdayTempListXX
  # move the new file back into place
  mv XXdatabaseAdayTempListXX databaseAdayList

  # give the file the correct permissions
  chmod 774 databaseAdayList
 
  exit 0
endif

