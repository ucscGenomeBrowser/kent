#!/bin/tcsh

################################
#  10-04-04
#  gets the names of all databases on an RR (or mgc) machine
#  using mark's genbank dumps.
#
################################

set databases=""
set rootpath="/cluster/data/genbank/var/tblstats"  # mark's TABLE STATUS dump
set mach1=""
set dirname1=""
set machpath1=""
set fullpath1=""

if ($#argv != 1) then
  echo
  echo "  gets the names of all databases on an RR (or mgc) machine"
  echo "  using mark's genbank dumps."
  echo
  echo "    usage: RRmachine"
  echo
  exit
else
  set mach1=$argv[1]
endif

set machpath1=$rootpath/$mach1

# check for valid machine names
if ($mach1 == hgwdev || $mach1 == hgwbeta) then
  echo
  echo "  $mach1 is not a RR node"
  echo
  exit 1
endif

if (! -e $machpath1 ) then
  echo
  echo "  $mach1 is not a valid machine"
  echo
  exit 1
endif




# set paths for most recent table status dump
set dirname1=`ls $machpath1 | sort -r | head -1`
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
  exit
endif

# get names of databases from files database.tbls in marks directories
ls -1 $fullpath1 | grep "tbls" | sed -e "s/\.tbls//" | sort

