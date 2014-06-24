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
# h = human readable
RSYNCOPTS="-ltrzvh"
# rsync server for CGIs and html files
RSYNCSRC="rsync://hgdownload.cse.ucsc.edu"
RSYNCCGIBIN=cgi-bin
RSYNCHTDOCS=htdocs

# check if running as root
if [ "$(id -u)" != "0" ] ; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

# check flag if run with no parameter (=from cron)
if [ "$#" -eq 0 ] ; then
   # check a flag file to see if anything on hgdownload actually changed
   if /root/urlIsNotNewerThanFile http://hgdownload.cse.ucsc.edu/gbib/lastUpdate /root/lastUpdateTime.flag
   then
       exit 0
   fi
fi

# use the right update script, depending on version
if [ "$1" == "alpha" -o "$1" == "beta" -o "$1" == "devbox" ] ; then
    UPDATEBASE=http://hgwdev.soe.ucsc.edu/gbib
else
    UPDATEBASE=http://hgdownload.cse.ucsc.edu/gbib
fi

# unless already calling self, update self and call self
if [ "$BASH_ARGV" != "noUpdate" ] ; then
    echo getting new update script
    wget $UPDATEBASE/updateBrowser.sh -O /root/updateBrowser.sh -q
    sudo /root/updateBrowser.sh $1 noUpdate
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
	
echo
echo Now updating the genome browser software and data:

# alpha states cannot use rsync on hgwdev, need to use tarball
if [ "$1" == "alpha" -o "$1" == "beta" -o "$1" == "devbox" ] ; then
    echo updating from prepared $1-package on genome-test
    pushd . > /dev/null
    cd /usr/local/apache
    wget http://genome-test.soe.ucsc.edu/browserbox/$1.tgz -O - | tar xvz
    cd /
    wget http://genome-test.soe.ucsc.edu/browserbox/$1-push.tgz -O - | tar xvz
    popd
# normal updates go via rsync
else
    # update CGIs
    echo updating CGIs...
    rsync --delete $RSYNCOPTS $RSYNCSRC/$RSYNCCGIBIN /usr/local/apache/cgi-bin/ --exclude=hg.conf --exclude edw* --exclude *private --exclude hgNearData/**

    echo updating HTML files...
    rsync --delete $RSYNCOPTS $RSYNCSRC/$RSYNCHTDOCS/ /usr/local/apache/htdocs/ --include **/customTracks/*.html --exclude ENCODE/ --exclude *.bw --exclude *.gz --exclude favicon.ico --exclude folders --exclude ancestors/** --exclude admin/** --exclude goldenPath/customTracks/*/* --exclude images/mammalPsg/** --exclude style/gbib.css --exclude images/title.jpg --exclude images/homeIconSprite.png
fi

chown -R www-data.www-data /usr/local/apache/cgi-bin/*
chown -R www-data.www-data /usr/local/apache/htdocs/

echo updating GBDB files...
rsync $RSYNCOPTS --existing rsync://hgdownload.cse.ucsc.edu/gbdb/ /data/gbdb/
chown -R www-data.www-data /data/gbdb/

echo updating MYSQL files - browser will not work during the MYSQL update
# inspired by http://forums.mysql.com/read.php?35,45577,47063#msg-47063
# it doesn't work if I use two mysql invocations, as 'flush tables with read lock'
# is only valid as long as the session is open
# so I use the SYSTEM command
echo "FLUSH TABLES WITH READ LOCK; SYSTEM rsync $RSYNCOPTS --existing rsync://hgdownload.cse.ucsc.edu/mysql/ /data/mysql/; SYSTEM chown -R mysql.mysql /data/mysql/; UNLOCK TABLES;" | mysql

echo updating hgcentral database
echo "FLUSH TABLES WITH READ LOCK; SYSTEM rsync $RSYNCOPTS --existing rsync://hgdownload.cse.ucsc.edu/mysql/hgcentral/ /data/mysql/hgcentral/; SYSTEM chown -R mysql.mysql /data/mysql/hgcentral; UNLOCK TABLES;" | mysql
# update blat servers
mysql hgcentral -e 'UPDATE blatServers SET host=CONCAT(host,".cse.ucsc.edu")'
# the box does not support HAL right now, remove the ecoli hubs
mysql hgcentral -e 'delete from hubPublic where hubUrl like "%nknguyen%"'

echo pulling other files
rsync $RSYNCOPTS hgdownload.cse.ucsc.edu::gbib/push/ /

echo patching menu 
cp /usr/local/apache/htdocs/inc/globalNavBar.inc /tmp/navbar.inc
# remove mirrors and downloads menu
sed -i '/<li class="menuparent" id="mirrors">/,/^<\/li>$/d' /tmp/navbar.inc 
sed -i '/<li class="menuparent" id="downloads">/,/^<\/li>$/d' /tmp/navbar.inc 
cat /tmp/navbar.inc | grep -v hgNear | grep -v hgVisiGene | sed -e '/hgLiftOver/a <li><a href="../cgi-bin/hgMirror">Mirror tracks</a></li>' | uniq > /usr/local/apache/htdocs/inc/globalNavBar.inc 
rm /tmp/navbar.inc

# patch left side menu:
# remove encode, neandertal, galaxy, visiGene, Downloads, cancer browser, microbial, mirrors, jobs
for i in hgNear ENCODE Neandertal galaxy VisiGene hgdownload genome-cancer microbes mirror jobs; do
    sed -i '/<A CLASS="leftbar" .*'$i'.*$/,/<HR>$/d' /usr/local/apache/htdocs/index.html
done

# patch main pages
sed -i 's/About the UCSC Genome Bioinformatics Site/About the UCSC Genome Browser in a Box/g' /usr/local/apache/htdocs/indexIntro.html
perl -0777 -pi -e 's/It also.+ provides.+> projects. //s' /usr/local/apache/htdocs/indexIntro.html
perl -0777 -pi -e 's/Program-driven.+ per day.//s' /usr/local/apache/htdocs/indexInfo.html
# patch contacts page
sed -i 's/......cgi-bin\/hgUserSuggestion/http:\/\/genome.ucsc.edu\/cgi-bin\/hgUserSuggestion/' /usr/local/apache/htdocs/contacts.html
# remove visigene from top menu
sed -i '/hgVisiGene/d' /usr/local/apache/htdocs/inc/home.topbar.html

# not needed, but running anyway
chown www-data.www-data /usr/local/apache/htdocs/inc/globalNavBar.inc

# make sure we have the right symlink to /media
sudo ln -sfT /media /usr/local/apache/htdocs/folders

# make sure this tableList is not there, it can break the box
mysql hgcentral -e 'drop table if exists tableList'

# hide the really big tracks
mysql hg19 -e 'update trackDb set visibility=0 where tableName like "cons%way"'
mysql hg19 -e 'update trackDb set visibility=0 where tableName like "ucscRetroAli%"'

# rsync tables on hgdownload are sometimes a in crashed state
echo checking mysql tables
sudo myisamchk --force --silent --fast --update-state /data/mysql/hg19/*.MYI /data/mysql/hgcentral/*.MYI /data/mysql/hgFixed/*.MYI 2> /dev/null

#LATENCY=`ping genome.ucsc.edu -n -c1 -q | grep rtt | cut -d' ' -f4 | cut -d/ -f2 | cut -d. -f1`
#if [ "$LATENCY" -gt "90" ]; then
#echo making low-latency changes
/usr/local/apache/cgi-bin/hgMirror postRsyncUpdates

touch /root/lastUpdateTime.flag
echo update done.
