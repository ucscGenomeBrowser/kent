#!/bin/tcsh
source `which qaConfig.csh`

################################
#  01-01-05
#  gets the filename of the TABLE STATUS dump from RR
#  using mark's genbank dumps.
#
#  Robert Kuhn
################################

set database=""
set rootpath="/cluster/data/genbank/var/tblstats"  # mark's TABLE STATUS dump
set machine="hgnfs1"
set dirname=""
set machpath=""
set dumpfile=""

if ( $#argv < 1 || $#argv > 2 ) then
  echo
  echo "  gets the filename of the TABLE STATUS dump from RR"
  echo "  using mark's genbank dumps."
  echo "    warning:  not in real time.  uses overnight dump."
  echo
  echo "    usage: database [hgwdev | hgwbeta | rr | hgnfs1]"
  echo
  echo "      defaults to rr.  optionally gives results for dev or beta"
  echo
  exit
else
  set database=$argv[1]
endif

if ( $#argv == 2 ) then
  set machine=$argv[2]
  echo $machine | egrep -q "hgwdev|hgwbeta|rr|hgnfs1|rrnfs1" 
  if ( $status ) then
    echo
    echo "  $machine not a valid machine" 
    echo
    exit 1
  endif
endif

if ( "rr" == $machine ) then
  set machine="hgnfs1"
endif

# set path to directory of dated dumps for the proper machine
set machpath=$rootpath/$machine

if (! -e $machpath ) then
  echo
  echo "  $machpath does not exist."
  echo
  exit 1
endif

# set path to latest stored TABLE STATUS dumps
set dirname=`ls -1 $machpath | tail -1`

set dumpfile=$machpath/$dirname/$database.tbls

if (! -e $dumpfile ) then
  echo
  echo "  database $database -- does not exist on $machine"
  echo 
  exit 1
else
  echo $dumpfile
endif 


set debug="false"
# set debug="true"
if ( $debug == "true" ) then
  echo
  echo "database = $database"
  echo "machine  = $machine"
  echo
  echo "machpath = $machpath"
  echo "dumpfile = $dumpfile"
  echo
  # exit
endif

exit

