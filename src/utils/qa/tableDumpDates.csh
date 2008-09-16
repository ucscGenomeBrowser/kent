#!/bin/tcsh

###############################################
#
#  09-16-2008
#  Ann Zweig
#
#  Checks to see if the tables are being dumped 
#  from the RR machines in a timely manner.
#
###############################################

set counter=( 1 2 3 4 5 6 7 8 )

if ($#argv != 1 ) then
  echo
  echo "  get date of latest table dumps from rr machines."
  echo
  echo "    usage: go"
  echo
  exit
endif

# run only from hgwdev
if ( "$HOST" != "hgwdev" ) then
  echo "\nERROR: you must run this script on hgwdev!\n"
  exit 1
endif

foreach i ( $counter )
  ls -lt /cluster/data/genbank/var/tblstats/hgw[$i] | head -n2 | egrep -v total \
  | awk '{print $9}'
end


