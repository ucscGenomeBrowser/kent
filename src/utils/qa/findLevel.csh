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
set dbs=`hgsql -e "SELECT name FROM dbDb" hgcentraltest | grep $db`
if ( "$dbs" != $db ) then
  echo "   Invalid database name.  Try again."
  echo
  exit
endif

# check trackDb for db/table combination 
hgsql -N -e 'SHOW TABLES' $db | grep -qi $tableName

if ($status) then
  echo "\n  no such database/table combination: $db $tableName.\n"
  exit
endif

##########################################
# find which trackDb.ra file the track is mentioned in
# start at the assembly level

if (! -e ~/trackDb/) then
  echo "\n  this program presumes you have a symlink to trackDb \
    in your home dir\n"
  exit
else
  cd ~/trackDb/*/$db 
endif
set currDir=`pwd`

grep -xq track.$tableName trackDb.ra

if (! $status ) then
  # the track is mentioned in the assembly-level trackDb.ra file

else
  # the track is not at the assembly-level, go up to the organism level
  cd ..
  set currDir=`pwd`
  grep -xq track.$tableName trackDb.ra

  if (! $status ) then
    # the track is mentioned in the organism-level trackDb.ra file

  else
    # the track is not at the organism level, go up to the top level
    cd ..
    set currDir=`pwd`
    grep -xq track.$tableName trackDb.ra
   
    if (! $status ) then
      # the track is mentioned in the top-level trackDb.ra file
    else 
       # the track is not at the top level either - it does not exist
       echo " * the $tableName track does not exist in any level trackDb.ra file"
       set currDir=""
    endif
  endif
endif
if ($currDir != "") then
  echo " * the $db $tableName track is located here: \
    `echo $currDir | sed 's^.*makeDb^~^'`/trackDb.ra"
endif
echo


###########################################
# now, find the level of the associated .html file
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
  echo " * the $db $tableName.html file is located here: \
    `echo $currDir | sed 's^.*makeDb^~^'`/$tableName.html"
endif
echo
