#!/bin/tcsh

################################
#  
#  08-04-2006
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
set status=""

if ( $#argv != 2 ) then
  echo
  echo " searches trackDb hierarchy for your table and corresponding .html file"
  echo " returns the lowest-level directory for each" 
  echo
  echo "    usage:  database tableName"
  echo
  exit
else
  set db=$argv[1]
  set tableName=$argv[2]
endif
echo

# make sure this is a valid database name
set dbs=`hgsql -e "select name from dbDb" hgcentraltest | grep $db`
if ( $dbs != $db ) then
  echo "   Invalid database name.  Try again."
  echo
  exit
endif


###########################################
# find which trackDb.ra file the track is mentioned in
# start at the assembly level
cd ~/trackDb/*/$db
set currDir=`pwd`

grep -xq track.$tableName trackDb.ra

if (! $status ) then
  # the track is mentioned in the assembly-level trackDb.ra file
  echo " * the $tableName track is located here: \
        `echo $currDir | sed 's^.*makeDb^~^'`/trackDb.ra"

else
  # the track is not at the assembly-level, go up to the organism level
  cd ..
  set currDir=`pwd`
  grep -xq track.$tableName trackDb.ra

  if (! $status ) then
    # the track is mentioned in the organism-level trackDb.ra file
    echo " * the $tableName track is located here: \
        `echo $currDir | sed 's^.*makeDb^~^'`/trackDb.ra"

  else
    # the track is not at the organism level, go up to the top level
    cd ..
    set currDir=`pwd`
    grep -xq track.$tableName trackDb.ra
   
    if (! $status ) then
      # the track is mentioned in the top-level trackDb.ra file
      echo " * the $tableName track is located here: \
        `echo $currDir | sed 's^.*makeDb^~^'`/trackDb.ra"

    else 
       # the track is not at the top level either - it does not exist
       echo " * the $tableName track does not exist in any level trackDb.ra file"
    endif
  endif
endif
echo


###########################################
# now, find the level of the associated .html file
# start at the assembly level
cd ~/trackDb/*/$db
set currDir=`pwd`

find $tableName.html >& /dev/null

if (! $status ) then
  # the .html file is found at the assembly-level
  echo " * the $tableName.html file is located here: \
        `echo $currDir | sed 's^.*makeDb^~^'`/$tableName.html"

else
  # the .html is not found at the assembly-level, go up to the organism level
  cd ..
  set currDir=`pwd`
  find $tableName.html >& /dev/null

  if (! $status ) then
    # the .html file is found at the organism-level
    echo " * the $tableName.html file is located here: \
        `echo $currDir | sed 's^.*makeDb^~^'`/$tableName.html"

  else
    # the .html file is not found at the organism level, go up to the top level
    cd ..
    set currDir=`pwd`
    find $tableName.html >& /dev/null

    if (! $status ) then
      # the .html file is found at the top-level
      echo " * the $tableName.html file is located here: \
        `echo $currDir | sed 's^.*makeDb^~^'`/$tableName.html"
    else
      # the .html file is not at the top level either - it does not exist
      echo " * the $tableName.html file does not exist at any level"
    endif
  endif
endif
echo
