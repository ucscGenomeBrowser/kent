#!/bin/tcsh
source `which qaConfig.csh`

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
set outPath='/usr/local/apache/htdocs-genecats/qa/test-results'
set todaysDb=''
set yestDb=''
set list=''
set error="ERROR - no database list. Try this first: databaseAday.csh init"

if ( $#argv < 1  || $#argv > 2 ) then
  echo
  echo " call with 'today' to find out the database we are monitoring today"
  echo
  echo "    usage:  today | yesterday | init | next [path]"
  echo "            path defaults to /usr/local/apache/htdocs-genecats/qa/test-results"
  echo
  exit 1
else
  set choice="$argv[1]"
endif

# run only from hgwdev
if ( "$HOST" != "hgwdev" ) then
  echo
  echo "\n ERROR: you must run this script on hgwdev!\n"
  exit 1
endif

if ( $#argv == 2 ) then
  set outPath=$argv[2]
endif

# check values of choice
echo $choice | egrep -wq "today|yesterday|init|next"
if ( $status ) then
  echo
  echo " you must call this script using either 'today', 'yesterday', \
    'init' or 'next'"
  echo
  exit 1
endif

# this is a 'today' call, just get the top db on the list and exit
if ( $choice == "today" ) then
  # make sure the databaseAdayList exists
  if (! -e $outPath/databaseAdayList) then
    echo "\n $error\n"
    exit 1
  endif
  # return the database at the top of the list
  set todaysDb=`head -1 $outPath/databaseAdayList`

  # be sure we haven't completely wrapped around the list
  # and redo list if so (catches any new dbs since last init)
  if ( $todaysDb == "DO-OVER" ) then
    $0 init $outPath
    set todaysDb=`$0 today $outPath`
    bash -c 'echo "starting new db list" >&2'
  endif
  # all is good!
  echo $todaysDb
  exit 0
endif

# this is a 'yesterday' call
if ( $choice == "yesterday" ) then
  if (! -e $outPath/databaseAdayList) then
    echo "\n $error\n"
    exit 1
  endif
  # return the database at the bottom of the list
  set yestDb=`tail -1 $outPath/databaseAdayList`
  # check to see if it is a new list
  # an redo list if so
  if ( $yestDb == "DO-OVER" ) then
    echo
    echo "  sorry, list is freshly generated."
    echo "  no db available from yesterday."
    echo
    exit 1
  else
    echo $yestDb
    exit 0
  endif
endif

# this is an initialization call -- query the RR to get the 
# list of active databases (replace the previous list (if any) with this new one)
if ( $choice == "init" ) then
  # remove existing file
  rm -f $outPath/databaseAdayList

  # get list of active $gbd databases
  hgsql -h $sqlrr hgcentral -B -N \
    -e "SELECT name FROM dbDb WHERE active = 1 ORDER BY RAND()" \
    > $outPath/databaseAdayList

  # add a tag at the end of the list 
  # (this will show when it's time to create a new list)
  echo "DO-OVER" >> $outPath/databaseAdayList 

  # give the file the correct permissions
  chmod 774 $outPath/databaseAdayList
  exit 0
endif

# this is a next call -- edit the list so that the next database is at the top
# this call happens automatically each night from a qateam cronjob
if ( $choice == "next" ) then
  # make sure the list exists
  if (! -e $outPath/databaseAdayList) then
    echo "\n $error\n"
    exit 1
  endif
  
  # get the database at the top of the list 
  set todaysDb=`head -1 $outPath/databaseAdayList`
  # be sure we haven't completely wrapped around the list
  # if so, reinitialize list and quit
  if ( $todaysDb == "DO-OVER" ) then
    $0 init
    exit 0
  endif
  # put the top db at the bottom of the list
  set list=`grep -v $todaysDb $outPath/databaseAdayList`
  echo $list $todaysDb | sed -e "s/ /\n/g" > $outPath/databaseAdayList
  exit 0
endif

