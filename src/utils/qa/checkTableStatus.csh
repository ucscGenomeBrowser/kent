#!/bin/tcsh

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
echo "hgw1: `ls -1 /cluster/data/genbank/var/tblstats/hgw1 | tail -1`"
echo "hgw2: `ls -1 /cluster/data/genbank/var/tblstats/hgw2 | tail -1`"
echo "hgw3: `ls -1 /cluster/data/genbank/var/tblstats/hgw3 | tail -1`"
echo "hgw4: `ls -1 /cluster/data/genbank/var/tblstats/hgw4 | tail -1`"
echo "hgw5: `ls -1 /cluster/data/genbank/var/tblstats/hgw5 | tail -1`"
echo "hgw6: `ls -1 /cluster/data/genbank/var/tblstats/hgw6 | tail -1`"
echo "hgw7: `ls -1 /cluster/data/genbank/var/tblstats/hgw7 | tail -1`"
echo "hgw8: `ls -1 /cluster/data/genbank/var/tblstats/hgw8 | tail -1`"
echo 


