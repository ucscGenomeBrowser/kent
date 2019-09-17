# qaConfig.bash
#
# This file is meant to be sourced by all of the qa scripts that use
# bash.  There is a separate qaConfig.csh file for tcsh scripts.
# It is a place to set variables, and probably to do all kinds of other
# useful stuff.

# variable containing the host of the mysql server for hgwbeta
sqlbeta=hgwbeta
sqlrr=genome-centdb
GENBANK="/cluster/data/genbank/etc/genbank.tbls" # location of official genbank table list
tblStatusDumps="rrnfs1" # location of genbank TABLE STATUS RR dump files
