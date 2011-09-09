#!/bin/csh -f
#
# This script checks NCBI's UniGene repository version, 
# downloads the latest and notifies $us if the version appears 
# to have changed.  
# Version state is maintained by the following files in 
# this directory:
# - current_version
# - previous_version
#

set us = "rhead ann fanhsu"
set localRepos = /hive/data/outside/uniGene
set path = (/usr/bin /bin)

cd $localRepos
mv current_version previous_version
rm -f Hs.info
wget -q ftp://ftp.ncbi.nih.gov/repository/UniGene/Homo_sapiens/Hs.info
sleep 10
if (! -e Hs.info) then
    echo "failed to wget Hs.info to $localRepos" | mail -s "UniGene ftp failed" $us
    mv previous_version current_version
    exit 
endif

set version = `perl -e '$t=<>;$t=~/\#(\d+)/;print "$1\n";' Hs.info`
echo $version > $localRepos/current_version

set prev = `cat $localRepos/previous_version`
if ("0$version" != "0$prev") then
    mkdir $localRepos/uniGene.$version
    if ($status) then
      echo "$0 failed to mkdir $localRepos/uniGene.$version" \
      | mail -s "failed to mkdir $localRepos/uniGene.$version" $us
      exit
    endif
    # cd to our uniGene download directory.
    cd $localRepos/uniGene.$version
    if ($status) then
      echo "$0 failed to cd $localRepos/uniGene.$version" \
      | mail -s "failed to cd $localRepos/uniGene.$version" $us
      exit
    endif
    set problem = 0
    wget -q ftp://ftp.ncbi.nih.gov/repository/UniGene/Homo_sapiens/Hs.info
    if ($status) set problem = 1
    wget -q ftp://ftp.ncbi.nih.gov/repository/UniGene/Homo_sapiens/Hs.seq.uniq.gz
    if ($status) set problem = 1
    wget -q ftp://ftp.ncbi.nih.gov/repository/UniGene/Homo_sapiens/Hs.data.gz
    if ($status) set problem = 1
    if ($problem) then
      echo "$0 got err from wget for at least one of the data files" \
      | mail -s "wget error in $localRepos/uniGene.$version" $us
      exit
    endif
    mail -s "Automated UniGene download to $localRepos/uniGene.$version" $us <<EOF
Automated notice -- a cron job noticed that the UniGene version 
appeared to have changed, and attempted to download the latest 
version.  The files Hs.info, Hs.seq.uniq.gz, and Hs.data.gz 
should be in the directory named on the Subject: line.  

Even if we aren't going to align this build to the latest human, we
should still keep it for a while, because it might turn out to be the
version that SAGEmap uses, and it could easily be replaced by the next
UniGene release by the time we want to download SAGEmap.

If we do want to align this build to the latest human, see the UniGene
alignment instructions in makeHg??.doc.  They're at the beginning of
the "MAKE UNIGENE ALIGNMENTS" section.

Cheers,

QA Team (actually Brooke, taking over for Angie)


EOF

endif
