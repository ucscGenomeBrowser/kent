#!/bin/tcsh

################################
#  10-12-04
#  gets the names of all tables from an RR (or mgc) database
#  using mark's genbank dumps.
#
#  Robert Kuhn
################################

set database=""
set rootpath="/cluster/data/genbank/var/tblstats"  # mark's TABLE STATUS dump
set mach1=""
set dirname1=""
set machpath1=""
set fullpath1=""

if ($#argv != 2) then
  echo
  echo "  gets the names of all tables from an RR (or mgc) database"
  echo "  using mark's genbank dumps."
  echo "    warning:  not in real time.  uses overnight dump."
  echo
  echo "    usage: RRmachine, database"
  echo
  exit
else
  set mach1=$argv[1]
  set database=$argv[2]
endif



# check for valid machines
if ($mach1 == hgwdev || $mach1 == hgwbeta) then
  echo
  echo "  this is for RR nodes only."
  echo "    usage: RRmachine, database"
  echo
  exit 1
else 
  set machpath1=$rootpath/$mach1
endif

if (! -e $machpath1 ) then
  echo
  echo "  $mach1 is not a valid machine"
  echo
  exit 1
endif

# set paths to stored TABLE STATUS dumps
set dirname1=`ls -ogtr $machpath1 | tail -1 | gawk '{print $7}'`
set fullpath1=$machpath1/$dirname1

set debug="false"
if ($debug == "true") then
  echo
  echo "mach1     $mach1"
  echo
  echo "machpath1 $machpath1"
  echo "dirname1  $dirname1"
  echo "fullpath1 $fullpath1"
  echo
  # exit
endif

# get the tablenames from Mark's TABLE STATUS dump
if (! -e $fullpath1/$database.tbls) then
  echo
  echo "  $database is not a valid database on $mach1"
  echo
else
  cat $fullpath1/$database.tbls | grep -v "Name" | \
      gawk '{print $1}' | sort
endif

