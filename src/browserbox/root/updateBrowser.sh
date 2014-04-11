#!/bin/bash

# update script on the box
# - updates itself from hgwdev and then run itself
# - updates cgis, html and gbdb via rsync 
# - updates hg.conf via wget
# - patches the menu
# - hides conservation+retro 
# - removes some searches if latency to UCSC is > 90msecs

# will not run if:
# - not run as root
# - if a script named updateBrowser.sh already is running
# - any hgMirror jobs are running
# - if a flagFile on hgDownload is not more recent than a local flag file



# rsync options:
# t = preserve time
# l = preserve symlinks
# p = preserve permissions
# r = recurse
# v = verbose
# z = compress
# P = permit partial download (for restart)
RSYNCOPTS="-ltrz"
# rsync server for CGIs and html files
RSYNCSRC="rsync://hgdownload.cse.ucsc.edu"
RSYNCCGIBIN=cgi-bin
RSYNCHTDOCS=htdocs

# check if running as root
if [ "$(id -u)" != "0" ] ; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

# if we didn't get a parameter this is the main update script
# check flag , update ourself and run with parameter to avoid recursion
if [ "$#" -eq 0 ] ; then
   # check a flag file to see if anything on hgdownload actually changed
   if /root/urlIsNotNewerThanFile http://hgdownload.cse.ucsc.edu/admin/hgcentral.sql /root/lastUpdateTime.flag
   then
       exit 0
   fi

    echo getting new update script
    wget http://hgwdev.soe.ucsc.edu/browserbox/updateBrowser.sh -O /root/updateBrowser.sh -q
    sudo /root/updateBrowser.sh noUpdate
    exit 0
fi

# check if running already, 3 = the main script + its update + the outer shell
RUNNING=`ps --no-headers -CupdateBrowser.sh | wc -l`
if [ ${RUNNING} -gt 3 ] ; then
    echo already running
    exit 1
fi
	
# check if any hgMirror jobs are running right now
# check if the group id file exists and also if any processes exist with this group id
# note that the .pid actually contains a group id, not a process id
if [ -f /tmp/lastJob.pid ] && [ "$(ps x -o pgid | grep $(cat /tmp/lastJob.pid) | wc -l)" != "0" ] ; then
    echo a hgMirror job is running right now, not updating
    exit 1
fi
	
if [ "$1" == "alpha" ] ; then
    echo updating from alpha server
    RSYNCSRC="rsync://hgwdev.soe.ucsc.edu"
    RSYNCOPTS="-ltrv"
    RSYNCCGIBIN=cgi-bin
    RSYNCHTDOCS=htdocs
fi

if [ "$1" == "beta" ] ; then
    echo updating from beta server
    RSYNCSRC="rsync://hgwdev.soe.ucsc.edu"
    RSYNCOPTS="-ltrv"
    RSYNCCGIBIN=cgi-bin-beta
    RSYNCHTDOCS=htdocs-beta
fi

echo
echo Now updating the genome browser programs, html files and data files:

# update CGIs
echo updating CGIs...
rsync --delete --delete-excluded $RSYNCOPTS $RSYNCSRC/$RSYNCCGIBIN /usr/local/apache/cgi-bin/ --exclude=hg.conf --exclude edw* --exclude *private --exclude hgNearData/**
chown -R www-data.www-data /usr/local/apache/cgi-bin/*

echo updating HTML files...
rsync --delete --delete-excluded $RSYNCOPTS $RSYNCSRC/$RSYNCHTDOCS/ /usr/local/apache/htdocs/ --include **/customTracks/*.html --exclude ENCODE/ --exclude *.bw --exclude *.gz --exclude favicon.ico --exclude folders --exclude ancestors/** --exclude admin/** --exclude goldenPath/customTracks/*/* --exclude images/mammalPsg/**
chown -R www-data.www-data /usr/local/apache/htdocs/
#wget http://hgwdev.soe.ucsc.edu/favicon.ico -O /usr/local/apache/htdocs/favicon.ico

echo updating GBDB files...
rsync $RSYNCOPTS --existing rsync://hgdownload.cse.ucsc.edu/gbdb/hg19/ /data/gbdb/hg19/
chown -R www-data.www-data /data/gbdb/

echo updating MYSQL files - browser will not work during the MYSQL update
# inspired by http://forums.mysql.com/read.php?35,45577,47063#msg-47063
# it doesn't work if I use two mysql invocations, as 'flush tables with read lock'
# is only valid as long as the session is open
# so I use the SYSTEM command
echo "FLUSH TABLES WITH READ LOCK; SYSTEM rsync $RSYNCOPTS --existing rsync://hgdownload.cse.ucsc.edu/mysql/hg19/ /data/mysql/hg19/; SYSTEM chown -R mysql.mysql /data/mysql/; UNLOCK TABLES;" | mysql

echo updating hgcentral database
echo "FLUSH TABLES WITH READ LOCK; SYSTEM rsync $RSYNCOPTS --existing rsync://hgdownload.cse.ucsc.edu/mysql/hgcentral/ /data/mysql/hgcentral/; SYSTEM chown -R mysql.mysql /data/mysql/hgcentral; UNLOCK TABLES;" | mysql
# update blat servers
mysql hgcentral -e 'UPDATE blatServers SET host=CONCAT(host,".cse.ucsc.edu")'
# the box does not support HAL right now, remove the ecoli hubs
mysql hgcentral -e 'delete from hubPublic where hubUrl like "%nknguyen%"'

echo patching menu 
cp /usr/local/apache/htdocs/inc/globalNavBar.inc /tmp/navbar.inc
cat /tmp/navbar.inc | grep -v hgNear | grep -v hgVisiGene | sed -e '/hgLiftOver/a <li><a href="../cgi-bin/hgMirror">Mirror tracks</a></li>' | uniq > /usr/local/apache/htdocs/inc/globalNavBar.inc 
rm /tmp/navbar.inc
# not needed, but running anyway
chown www-data.www-data /usr/local/apache/htdocs/inc/globalNavBar.inc

# make sure we have the current hg.conf
sudo wget -q http://hgwdev.soe.ucsc.edu/browserbox/hg.conf -O /usr/local/apache/cgi-bin/hg.conf

# make sure we have the right symlink to /media
sudo ln -sfT /media /usr/local/apache/htdocs/folders

# make sure this tableList is not there, it can break the box
mysql hgcentral -e 'drop table if exists tableList'

# hide big tracks
mysql hg19 -e 'update trackDb set visibility=0 where tableName like "cons%way"'
mysql hg19 -e 'update trackDb set visibility=0 where tableName like "ucscRetroAli%"'

LATENCY=`ping genome.ucsc.edu -n -c1 -q | grep rtt | cut -d' ' -f4 | cut -d/ -f2 | cut -d. -f1`
if [ "$LATENCY" -gt "90" ]; then
    echo making low-latency changes
    # excluding some rarely used tracks from track search
    # potentially add: rgdQtl rgdRatQtl hgIkmc acembly jaxQtlAsIs jaxQtlPadded vegaGene
    /usr/local/apache/cgi-bin/hgMirror hideTracks
fi

touch /root/lastUpdateTime.flag
echo update done.
