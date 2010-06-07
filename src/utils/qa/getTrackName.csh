#!/bin/tcsh
source `which qaConfig.csh`

################################
#  
#  07-31-2007
#  Ann Zweig
#
#  give the script a table name and find out
#  the short label and group of the track it belongs to
#
################################


set db=""
set tableName=""
set parentTableName=""
set parentTrack=""
set subTrack=""
set superName=""
set superTrack=""
set superLabel=""
set grp=""
set grpName=""

if ( $#argv != 2 ) then
  echo
  echo " Returns the short label and group of the track for this table."
  echo " In the case of a composite track, it returns the short label"
  echo " for both the sub track and the parent track."
  echo
  echo "    usage:  database tableName"
  echo
  exit
else
  set db=$argv[1]
  set tableName=$argv[2]
  set superName=$tableName
endif
echo

if ( "$HOST" != "hgwdev" ) then
 echo "\n ERROR: You must run this script on dev!\n"
 exit 1
endif

# make sure this is a valid database name
set dbs=`hgsql -e "SELECT name FROM dbDb" hgcentraltest | grep -w $db`
if ( "$dbs" != $db ) then
  echo
  echo "\n ERROR: Invalid database name.  Try again.\n"
  echo
  exit 1
endif

#find the shortLabel and group for this table
set subTrack=`hgsql -Ne "SELECT shortLabel FROM trackDb WHERE tableName \
  LIKE '$tableName'" $db`
if ( "" == "$subTrack" ) then
  echo "\n ERROR: could not find the $tableName table in the $db database\n"
  exit 1
else
  echo " table:  $tableName"
  echo
  echo " name:   $subTrack"
  set grp=`hgsql -Ne "SELECT grp FROM trackDb WHERE tableName \
    LIKE '$tableName'" $db`
endif

#find out if this table is a sub-track
set parentTableName=`hgsql -Ne "SELECT settings FROM trackDb WHERE tableName \
  LIKE '$tableName' AND settings like '%subTrack%'" $db \
  | sed -e "s/\\n/\n/g" | grep subTrack | awk '{print $2}'`

if ( "" != "$parentTableName" ) then
  #get the Short Label for the parent track
  set parentTrack=`hgsql -Ne "SELECT shortLabel FROM trackDb WHERE tableName \
    LIKE '$parentTableName'" $db`
  echo " parent: $parentTrack"
  
  # reset the superName to use the parent to find superTrack
  set superName=$parentTableName
endif

# get the superTrack and superLabel, if any 
set superTrack=`hgsql -Ne 'SELECT settings FROM trackDb \
  WHERE tableName = "'$superName'"' $db | sed "s/\\n/\n/g" \
  | grep superTrack | awk '{print $2}'`
set superLabel=`hgsql -Ne 'SELECT shortLabel FROM trackDb \
  WHERE tableName = "'$superTrack'"' $db`

# output supertrack info
if ( "" != "$superLabel" ) then
  echo " super:  ${superLabel}..."
endif

#report about the group that this track is in
 set grpName=`hgsql -Ne "SELECT label FROM grp WHERE name LIKE '$grp'" $db`
 echo " group:  $grpName\n"

exit 0
