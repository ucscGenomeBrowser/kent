#!/bin/bash

# update script on the box
# - updates itself and then run itself
# - updates cgis, html and gbdb via rsync 
# - updates hg.conf via wget
# - patches the menu
# - hides conservation+retro 
# - removes some searches if latency to UCSC is > 90msecs

# will not run if:
# - not run as root
# - if a script named updateBrowser.sh already is running
# - any hgMirror jobs are running
# - hgdownload is offline
# - if a flagFile on hgDownload is not more recent than a local flag file

# parameters:
# - parameter "hgwdev": does not update itself, copies only the beta/alpha CGIs/htdocs from hgwdev
# - parameter "notSelf": does not update itself and does not check flagfile

# rsync options:
# l = preserve symlinks
# t = preserve time
# r = recurse
# z = compress
# v = verbose
# h = human readable
# u = skip if file is newer on receiver
RSYNCOPTS="-ltrzvh"
# rsync server for CGIs and html files
RSYNCSRC="rsync://hgdownload.cse.ucsc.edu"
RSYNCCGIBIN=cgi-bin
RSYNCHTDOCS=htdocs
UPDATEFLAG=http://hgdownload.cse.ucsc.edu/gbib/lastUpdate

# help
if [ "$1" == "-h" ] ; then
   echo "Without any options, this script checks if it can reach hgdownload and if "
   echo "new data has been added since the last run. It updates itself and runs the new copy."
   echo "The new copy rsyncs the CGIs/MysqlDbs/htdocs from hgdownload."
   echo "It rsyncs the gbib/push directory into / to update other files."
   echo "It finally repairs mysql tables, makes some trackDb changes, adds symlinks,"
   echo "and chmods the directories."
   echo "Parameters:"
   echo "updateBrowser.sh notSelf - do not update the script itself. Do not check if data has been "
   echo "                           added to hgdownload since the last run"
   echo "updateBrowser.sh hgwdev - more info on how to update to alpha versions (used only by UCSC)"
   exit 0
fi

# check if running as root
if [ "$(id -u)" != "0" ] ; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

# check if running already, 3 = the main script + its update + the subshell where this command is running
RUNNING=`ps --no-headers -CupdateBrowser.sh | wc -l`
if [ ${RUNNING} -gt 3 ] ; then
    #echo update already running
    exit 2
fi
	
# check if we have internet
wget -q --tries=1 --timeout=10 --spider http://hgdownload.soe.ucsc.edu -O /dev/null
if [ $? -ne 0 ] ; then
    exit 3
fi

# check flag if run with no parameter (=from cron)
if [ "$#" -eq 0 ] ; then
   # check a flag file to see if anything on hgdownload actually changed
   if /root/urlIsNotNewerThanFile $UPDATEFLAG /root/lastUpdateTime.flag
   then
       exit 0
   fi
fi

# unless already calling self, update self and call self unless doing only cgis
if [ "$BASH_ARGV" != "notSelf" -a "$1" != "hgwdev" ] ; then
    echo getting new update script
    # we got three VMs where updateBrowser.sh was 0 bytes, so doing download/move now
    wget http://hgdownload.soe.ucsc.edu/gbib/updateBrowser.sh -O /root/updateBrowser.sh.new -q && mv /root/updateBrowser.sh.new /root/updateBrowser.sh
    chmod a+x /root/updateBrowser.sh
    /root/updateBrowser.sh $1 notSelf
    exit 0
fi

# check if any hgMirror jobs are running right now
# check if the group id file exists and also if any processes exist with this group id
# note that the .pid actually contains a group id, not a process id
if [ -f /tmp/lastJob.pid ] && [ "$(ps x -o pgid | grep $(cat /tmp/lastJob.pid) | wc -l)" != "0" ] ; then
    echo a hgMirror job is running right now, not updating
    exit 4
fi
	
echo
echo Updating the genome browser software via rsync:

# CGI-BIN and HTDOCS:
# the parameter "hgwdev" copies over only the beta/alpha CGIs from hgwdev
if [ "$1" == "hgwdev" ] ; then
    # note the missing -u option to RSYNC: in hgwdev mode, we want to overwrite everything.
    # On a development machine, the developer might have touched a file
    # for testing. We want to make sure that all local files are overwritten by the 
    # files on hgwdev
    RSYNCOPTS="-ltrzvh"
    user=$2
    dirExt=$3

    if [ "$user" == "" ]; then
        echo arguments: updateBrowser hgwdev hgwdevUsername cgiDirectoryExtension 
        echo in alpha/beta mode you need to provide a username for the hgwdev login
        echo and a directory extension, the part after /usr/local/apache/cgi-bin-XXX
        echo The extension '"alpha"' is translated to '"no extension"'
        echo example '"updateBrowser hgwdev kent alpha"'
        echo example '"updateBrowser hgwdev hiram beta"'
        exit 5
    fi

    if [ "$dirExt" == "alpha" ] ; then
    	cgiExt=""
    	htmlExt="" 
    else
    	cgiExt="-"$dirExt
    	htmlExt="-"$dirExt
    fi

    RSYNCAPACHE="$RSYNCOPTS --delete"

     # remove a lot of clutter that accumulated in hgwdev's alpha cgi-bin dir
    if [ "$dirExt" == "alpha" ] ; then
      RSYNCAPACHE="$RSYNCAPACHE --exclude ENCODE/**.pdf --exclude *.gz --exclude *.bw --exclude *.bb --exclude *.bam --exclude goldenPath/**.pdf --exclude admin/** --exclude goldenPath/customTracks/** --exclude pubs/** --exclude ancestors/** --exclude training/** --exclude trash --exclude style-public/** --exclude js-public/** --exclude **/edw* --exclude images/mammalPsg/** --exclude **/encodeTestHub* --exclude favicon.ico --exclude folders --exclude ENCODE/** --exclude ENCODE/** --exclude Neandertal/** --exclude gbib/** --exclude generator/** --exclude js-*/** --exclude js/*/* --exclude .\* --exclude x86_64/* --exclude .xauth --exclude .hg.conf --exclude hgHeatmap* --exclude hg.conf --exclude 'hgt/**' --exclude admin/** --exclude images --exclude trash --exclude edw* --exclude visiGeneData/** --exclude crom_dir/ --exclude testp/ --exclude *.exe --exclude *.old --exclude *.tmp --exclude *.bak --exclude test* --exclude hg.conf* --exclude **/hgHeatmap* --exclude ~* --exclude Intronerator** --exclude hgText --exclude hgSubj --exclude gisaid* --exclude nt4.dir --exclude qaPush* --exclude docIdView --exclude ct/ --exclude *.bak --exclude hg.conf* --exclude gsid*/ --exclude *.private --exclude useCount --exclude ~* --exclude lssnp/"
    fi
  
    # remove things that are on hgwdev beta directories but not necessary on the gbib
    if [ "$dirExt" == "beta" ] ; then
      RSYNCAPACHE="$RSYNCAPACHE --exclude favicon*.ico --exclude hg.conf* --exclude ENCODE --exclude *.gz --exclude *.bw --exclude *.bb --exclude *.bam --exclude goldenPath/**.pdf --exclude admin --exclude goldenPath/customTracks --exclude pubs --exclude ancestors --exclude training --exclude trash --exclude .htaccess --exclude htdocs --exclude Neandertal --exclude RNA-img --exclude ebola --exclude encodeDCC --exclude evoFold --exclude geneExtra --exclude js-public --exclude style-public --exclude hgNearData --exclude visiGeneData --exclude visiGene"
    fi

    rsync $RSYNCAPACHE $user@hgwdev.soe.ucsc.edu:/usr/local/apache/htdocs${htmlExt}/ /usr/local/apache/htdocs/
    rsync $RSYNCAPACHE $user@hgwdev.soe.ucsc.edu:/usr/local/apache/cgi-bin${cgiExt}/ /usr/local/apache/cgi-bin/

    PUSHLOC=$user@hgwdev.soe.ucsc.edu:/usr/local/apache/htdocs/gbib/push/

# normal public updates from hgdownload are easier, not many excludes necessary
else
    # update CGIs
    echo updating CGIs...
    rsync --delete $RSYNCOPTS $RSYNCSRC/$RSYNCCGIBIN /usr/local/apache/cgi-bin/ --exclude=hg.conf --exclude edw* --exclude *private --exclude hgNearData --exclude visiGeneData --exclude Neandertal 

    echo updating HTML files...
    rsync --delete $RSYNCOPTS $RSYNCSRC/$RSYNCHTDOCS/ /usr/local/apache/htdocs/ --include **/customTracks/*.html --exclude ENCODE/ --exclude *.bam --exclude *.bb --exclude */*.bw --exclude */*.gz --exclude favicon.ico --exclude folders --exclude ancestors --exclude admin --exclude goldenPath/customTracks --exclude images/mammalPsg --exclude style/gbib.css --exclude images/title.jpg --exclude images/homeIconSprite.png --exclude goldenPath/**.pdf --exclude training

    PUSHLOC=hgdownload.cse.ucsc.edu::gbib/push/
fi

chown -R www-data.www-data /usr/local/apache/cgi-bin/*
chown -R www-data.www-data /usr/local/apache/htdocs/
chmod -R a+r /usr/local/apache/htdocs

if [ "$1" != "hgwdev" ] ; then
  echo updating GBDB files...
  rsync $RSYNCOPTS --existing rsync://hgdownload.cse.ucsc.edu/gbdb/ /data/gbdb/
  chown -R www-data.www-data /data/gbdb/
fi

echo pulling other files
rsync $RSYNCOPTS $PUSHLOC /

if [ "$1" != "hgwdev" ] ; then
  echo updating MYSQL files - browser will not work during the MYSQL update
  # inspired by http://forums.mysql.com/read.php?35,45577,47063#msg-47063
  # it doesn't work if I use two mysql invocations, as 'flush tables with read lock'
  # is only valid as long as the session is open
  # so I use the SYSTEM command
  echo "FLUSH TABLES WITH READ LOCK; SYSTEM rsync $RSYNCOPTS --existing rsync://hgdownload.cse.ucsc.edu/mysql/ /data/mysql/; SYSTEM chown -R mysql.mysql /data/mysql/; UNLOCK TABLES;" | mysql
  
  echo updating hgcentral database, make sure to always overwrite
  echo "FLUSH TABLES WITH READ LOCK; SYSTEM rsync -vrz --existing rsync://hgdownload.cse.ucsc.edu/mysql/hgcentral/ /data/mysql/hgcentral/; SYSTEM chown -R mysql.mysql /data/mysql/hgcentral; UNLOCK TABLES;" | mysql
  # update blat servers
  mysql hgcentral -e 'UPDATE blatServers SET host=CONCAT(host,".cse.ucsc.edu") WHERE host not like "%ucsc.edu"'
  # the box does not officially support the HAL right now, remove the ecoli hubs
  mysql hgcentral -e 'delete from hubPublic where hubUrl like "%nknguyen%"'
fi

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
perl -0777 -pi -e 's/It also.+ provides.+> project. //s' /usr/local/apache/htdocs/indexIntro.html
perl -0777 -pi -e 's/Program-driven.+ per day.//s' /usr/local/apache/htdocs/indexInfo.html
# patch contacts page
sed -i 's/......cgi-bin\/hgUserSuggestion/http:\/\/genome.ucsc.edu\/cgi-bin\/hgUserSuggestion/' /usr/local/apache/htdocs/contacts.html
# remove visigene from top menu
sed -i '/hgVisiGene/d' /usr/local/apache/htdocs/inc/home.topbar.html

# maybe not needed, but running anyway
chown www-data.www-data /usr/local/apache/htdocs/inc/globalNavBar.inc

# make sure we have the right symlink to /media
sudo ln -sfT /media /usr/local/apache/htdocs/folders
sudo ln -sfT /data/trash /usr/local/apache/htdocs/trash

# make sure this tableList is not there, it can break the box
# hgcentral on hgdownload has tables missing: those with users and passwords
mysql hgcentral -e 'drop table if exists tableList'

# hide the really big tracks
mysql hg19 -e 'update trackDb set visibility=0 where tableName like "cons%way"'
mysql hg19 -e 'update trackDb set visibility=0 where tableName like "ucscRetroAli%"'

# temporary fix for hgdownload problem, Oct 2014
ls /data/mysql/eboVir3 > /dev/null 2> /dev/null && mysql eboVir3 -e 'drop table if exists history'

# rsync tables on hgdownload are sometimes in a crashed state
echo checking mysql tables
#sudo myisamchk --force --silent --fast /data/mysql/hg19/*.MYI /data/mysql/hgcentral/*.MYI /data/mysql/hgFixed/*.MYI 2> /dev/null
mysqlcheck --all-databases --auto-repair --quick --fast --silent

#LATENCY=`ping genome.ucsc.edu -n -c1 -q | grep rtt | cut -d' ' -f4 | cut -d/ -f2 | cut -d. -f1`
#if [ "$LATENCY" -gt "90" ]; then
#echo making low-latency changes
/usr/local/apache/cgi-bin/hgMirror postRsyncUpdates

touch /root/lastUpdateTime.flag
echo update done.
cat /etc/issue | tr -s '\n'
