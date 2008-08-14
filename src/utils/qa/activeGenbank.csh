#!/bin/tcsh

################################
#
# Compare the list of active assemblies on the RR with the list
# of the assemblies on the RR genbank update list.
#
################################

# these are assemblies that are too old for nightly genbank updates
set exceptions='ce1|hg15|rn2|mm5|mm6'

# usage statement
if ( $argv != go ) then
  echo
  echo " Finds diffs between active DBs and DBs getting genbank updates."
  echo " Only works for RR databases."
  echo
  echo "  usage: go"
  echo
  exit
endif

# run this only on hgwdev
  if ( "$HOST" != "hgwdev" ) then
   echo "\n error: you must run this script on dev!\n"
   exit 1
  endif

# this is the list of DBs on the RR getting genbank updates
cat ~/kent/src/hg/makeDb/genbank/etc/rr.dbs | sort | sed -e '/^#/ d' \
> XXgenbankDbsXX

# this is the list of active DBs on the RR
hgsql -N -h genome-centdb hgcentral -e "SELECT name FROM dbDb \
WHERE active = 1 ORDER BY name;" > XXactiveDbsXX

# find out what's active and not getting a genbank update
# disregard those assemblies that are too old for updates
comm -13 XXgenbankDbsXX XXactiveDbsXX | egrep -v $exceptions

# clean-up
rm XXgenbankDbsXX
rm XXactiveDbsXX
