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
echo " hgwdev: `ls -1 /cluster/data/genbank/var/tblstats/hgwdev | tail -1`"
echo "hgwbeta: `ls -1 /cluster/data/genbank/var/tblstats/hgwbeta | tail -1`"
echo " rrnfs1: `ls -1 /cluster/data/genbank/var/tblstats/rrnfs1 | tail -1`"
echo 


