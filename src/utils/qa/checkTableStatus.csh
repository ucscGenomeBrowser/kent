#!/bin/tcsh
source `which qaConfig.csh`

################################
#  10-03-06
#
#  Checks the date for the last TABLE STATUS dump for 
#  dev, beta and each of the 8 RR machines.
#
################################

echo "TABLE STATUS files were last dumped:"
echo " hgwdev: `ls -1 /hive/data/outside/genbank/var/tblstats/hgwdev | tail -1`"
echo "hgwbeta: `ls -1 /hive/data/outside/genbank/var/tblstats/hgwbeta | tail -1`"
echo "     rr: `ls -1 /hive/data/outside/genbank/var/tblstats/$tblStatusDumps | tail -1`"
echo 

