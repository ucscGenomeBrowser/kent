#!/bin/tcsh

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
set encode=""
set split=""
set partName=""
set matches=""
set match=""
set prio=""
set vis=""

if ( $#argv < 2  || $#argv > 4 ) then
  echo
  echo " searches trackDb hierarchy for your table and corresponding .html file"
  echo " optionally returns the location of the priority and visibility entries"
  echo " returns the lowest-level directory for each" 
  echo
  echo "    usage:  database tableName [priority] [visibility]"
  echo
  exit
else
  set db=$argv[1]
  set tableName=$argv[2]
endif
echo

# find out if they want priority and/or visibility as well
if ( $#argv == 3 ) then
  if ( $argv[3] == 'priority' ) then
    set prio="T"
  endif
  if ( $argv[3] == 'visibility' ) then
    set vis="T"
  endif
endif
if ( $#argv == 4 ) then
  set prio="T"
  set vis="T"
endif

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

# detect split chroms and check trackDb for db/table combination 
set split=`getSplit.csh $db $tableName hgwdev`'_'
hgsql -N -e 'SHOW TABLES' $db | egrep -wqi "${split}$tableName|$tableName"
if ($status) then 
  # look for ref to composite main entries which have no tables
  find ../../ -name "trackDb*ra" | xargs grep -wq "subTrack.$tableName" 
  if ($status) then
    echo " ERROR: No such database/table combination: $db $tableName"
    echo "  and no composite track by that name.\n"
    exit
  else
    echo "  Composite track. No single table by this name: $tableName"
    echo
  endif
else
  find ../../ -name "trackDb*ra" | xargs grep -xq "track.$tableName" 
  if ($status) then
    echo "  This is a composite subtrack table"
    echo
  endif
endif

# check for entry in trackDb.ra, starting at assembly level
set matches=`grep -ws "track.$tableName" trackDb.ra`
set match=`echo $matches | grep -v "^#"`
if ( "$match" != '' ) then
  # the track is mentioned in the assembly-level trackDb.ra file
else
  # see if it's in another assembly-level trackDb*
  set partName=`grep -wHs track.$tableName trackDb.*.ra`
  if ( "$partName" != '' ) then
    # the track is mentioned in an assembly-level trackDb.*.ra file
    set encode=`echo $partName | sed -e "s/trackDb//" | sed -e "s/.ra:track $tableName//"`
  else 
    # the track is not at the assembly-level, go up to the organism level
    cd ..
    set currDir=`pwd`
    set matches=`grep -ws track.$tableName trackDb.ra`
    set match=`echo $matches | grep -v "^#"`
    if ( "$match" != '' ) then
      # the track is mentioned in the organism-level trackDb.ra file
    else
      # see if it's in another organism-level trackDb.*.ra
      set partName=`grep -wHs track.$tableName trackDb.*.ra`
      if ( "$partName" != '' ) then
        # the track is mentioned in an organism-level trackDb.*.ra file
        set encode=`echo $partName | sed -e "s/trackDb//" | sed -e "s/.ra:track $tableName//"`
      else
        # the track is not at the organism level, go up to the top level
        cd ..
        set currDir=`pwd`
        set matches=`grep -ws track.$tableName trackDb.ra`
        set match=`echo $matches | grep -v "^#"`
        if ( "$match" != '' ) then
          # the track is mentioned in the top-level trackDb.ra file
        else 
           # the track is not at the top level either - it does not exist
           echo " * the $tableName track does not exist in any level trackDb.ra file"
           set currDir=""
        endif
      endif
    endif
  endif
endif
if ($currDir != "") then
  echo " * trackDb: \
    `echo $currDir | sed -e 's^.*makeDb^~^'`/trackDb$encode.ra"
endif
echo

###########################################
# check for entry in priority.ra file, starting at assembly level
if ( "T" == $prio) then
  # start back at the assembly level
  cd ~/trackDb/*/$db 
  set currDir=`pwd`
  set matches=`grep -ws "track.$tableName" priority.ra`
  set match=`echo $matches | grep -v "^#"`
  if ( "$match" != '' ) then
    # the track is mentioned in the assembly-level priority.ra file
  else
    # the track is not at the assembly-level, go up to the organism level
    cd ..
    set currDir=`pwd`
    set matches=`grep -ws track.$tableName priority.ra`
    set match=`echo $matches | grep -v "^#"`
    if ( "$match" != '' ) then
      # the track is mentioned in the organism-level priority.ra file
    else
      # the track is not at the organism level, go up to the top level
      cd ..
      set currDir=`pwd`
      set matches=`grep -ws track.$tableName priority.ra`
      set match=`echo $matches | grep -v "^#"`
      if ( "$match" != '' ) the
        # the track is mentioned in the top-level priority.ra file
      else 
        # the track is not at the top level either
        echo " * priority:"
        echo " The $db.$tableName track entry does not exist in any level"
        echo " priority.ra file. This doesn't mean there is no priority"
        echo " entry -- it's probably in the trackDb.ra file."
        set currDir=""
      endif
    endif
  if ($currDir != "") then
    if (-e $currDir/priority.ra) then
      echo " * priority: \
        `echo $currDir | sed -e 's^.*makeDb^~^'`/priority.ra"
    else
      echo " * priority:"
      echo " The $db.$tableName track entry does not exist in any level"
      echo " priority.ra file. This doesn't mean there is no priority"
      echo " entry -- it's probably in the trackDb.ra file."
    endif
  endif
  echo
endif #end find priority entry

###########################################
# check for entry in visibility.ra file, starting at assembly level
if ( "T" == $vis) then
  # start back at the assembly level
  cd ~/trackDb/*/$db 
  set currDir=`pwd`
  set matches=`grep -ws "track.$tableName" visibility.ra`
  set match=`echo $matches | grep -v "^#"`
  if ( "$match" != '' ) then
    # the track is mentioned in the assembly-level visibility.ra file
  else
    # the track is not at the assembly-level, go up to the organism level
    cd ..
    set currDir=`pwd`
    set matches=`grep -ws track.$tableName visibility.ra`
    set match=`echo $matches | grep -v "^#"`
    if ( "$match" != '' ) then
      # the track is mentioned in the organism-level visibility.ra file
    else
      # the track is not at the organism level, go up to the top level
      cd ..
      set currDir=`pwd`
      set matches=`grep -ws track.$tableName visibility.ra`
      set match=`echo $matches | grep -v "^#"`
      if ( "$match" != '' ) the
        # the track is mentioned in the top-level visibility.ra file
      else 
        # the track is not at the top level either
        echo " * visibility:"
        echo " The $db.$tableName track entry does not exist in any level"
        echo " visibility.ra file. This doesn't mean there is no visibility"
        echo " entry -- it's probably in the trackDb.ra file."
        set currDir=""
      endif
    endif
  if ($currDir != "") then
    if (-e $currDir/visibility.ra) then
      echo " * visibility: \
        `echo $currDir | sed -e 's^.*makeDb^~^'`/visibility.ra"
    else
      echo " * visibility:"
      echo " The $db.$tableName track entry does not exist in any level"
      echo " visibility.ra file. This doesn't mean there is no visibility"
      echo " entry -- it's probably in the trackDb.ra file."
    endif
  endif
  echo
endif #end find visibility entry

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
  echo " * html file: \
    `echo $currDir | sed 's^.*makeDb^~^'`/$tableName.html"
endif
echo

exit 0
