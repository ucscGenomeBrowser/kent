# qaConfig.csh
#
# This file is meant to be sourced by all of the qa scripts that use
# tcsh.  There is a separate qaConfig.bash file for bash scripts.
# It is a place to set variables, and probably to do all kinds of other
# useful stuff.

# variable containing the host of the mysql server for hgwbeta
set sqlbeta = hgwbeta
set sqlrr   = genome-centdb
set GENBANK="/cluster/data/genbank/etc/genbank.tbls"  # location of official genbank table list
set tblStatusDumps="hgnfs1" # location of genbank TABLE STATUS RR dump files
