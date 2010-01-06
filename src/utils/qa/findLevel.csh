#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  09-26-2007
#  Ann Zweig
#
#  find out which level in the trackDb directory
#  a track is on, and which level the corresponding
#  .html file is on.
#
################################


set db=""
set tableName=""
set currDir=""
set dbs=""

if ( $#argv != 2 ) then
  echo
  echo " searches trackDb hierarchy for your table and corresponding .html file"
  echo " also returns the value of the priority and visibility entries"
  echo " and the .ra file location for each" 
  echo
  echo "    usage:  database tableName"
  echo
  exit
else
  set db=$argv[1]
  set tableName=$argv[2]
endif
echo

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

# make sure this is a valid database name
set dbs=`hgsql -e "SELECT name FROM dbDb" hgcentraltest | grep -w $db`
if ( "$dbs" != $db ) then
  echo
  echo "   Invalid database name.  Try again."
  echo
  exit
endif

# check for trackDb dir at $USER root
file ~/trackDb | grep -q 'symbolic link'
if ( $status ) then
  echo "\n  this program presumes you have a symlink to trackDb \
    in your home dir\n"
  exit
else
  # start at the assembly level
  cd ~/trackDb/*/$db 
  set currDir=`pwd`
endif

###########################################
# find the level of the associated .html file
# start at the assembly level
cd ~/trackDb/*/$db
set currDir=`pwd`

if (-e $tableName.html) then
  # the .html file is found at the assembly-level

else
  # the .html is not found at the assembly-level, go up to the organism level
  cd ..
  set currDir=`pwd`
  if (-e $tableName.html) then
    # the .html file is found at the organism-level

  else
    # the .html file is not found at the organism level, go up to the top level
    cd ..
    set currDir=`pwd`
    if (-e $tableName.html) then
      # the .html file is found at the top-level
    else
      # the .html file is not at the top level either - it does not exist
      echo " * the $tableName.html file does not exist at any level"
      set currDir=""
    endif
  endif
endif
if ($currDir != "") then
  echo " * html file: \
    `echo $currDir | sed 's^.*makeDb^~^'`/$tableName.html"
endif
echo


###########################################
# find the trackDb.ra entry (using tdbQuery)

echo " * trackDb:"
tdbQuery "select track,priority,visibility,filePos from $db" \
 | grep -w -A3 "$tableName"

exit 0
