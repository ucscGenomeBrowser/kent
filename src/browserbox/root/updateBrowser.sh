#!/bin/bash

# update script on the GBiB virtual machine
# - updates itself and then run itself
# - updates cgis, html and gbdb via rsync 
# - patches the menu
# - calls hgMirror to hide some slow default tracks, e.g. conservation+retro 

# will not run if:
# - not run as root
# - if a script named updateBrowser.sh already is running
# - any hgMirror jobs are running
# - hgdownload is offline
# - if a flagFile on hgDownload is not more recent than a local flag file
# - if the VirtualBox Guest property "gbibAutoUpdateOff" is set. To set it, run this on the host 
#   "VBoxManage guestproperty set browserbox gbibAutoUpdateOff"

# To find out why the script did not run, use the command echo $? to show the
# exit code of the script:
# 1 - not root
# 2 - already running
# 3 - no internet connection
# 4 - hgMirror job is running
# 5 - not enough arguments
# 6 - virtualbox guest property is set

# parameters:
# - parameter "hgwdev": does not update itself, copies only the beta/alpha CGIs/htdocs from hgwdev
# - parameter "notSelf": does not update itself and does not check flagfile

# this script is not using the bash options pipefail or errabort.
# In case something goes wrong it continues, this is intentional to 
# avoid a virtual machine that can not update itself anymore

# rsync options:
# l = preserve symlinks
# t = preserve time
# r = recurse
# z = compress
# v = verbose
# h = human readable
# u = skip if file is newer on receiver
# We are not using the -z option anymore because it may cause
# CPU overload on hgdownload
RSYNCOPTS="-ltrvh"
# rsync server for CGIs and html files
RSYNCSRC="rsync://hgdownload.cse.ucsc.edu"
RSYNCCGIBIN=cgi-bin
RSYNCHTDOCS=htdocs
UPDATEFLAG=http://hgdownload.cse.ucsc.edu/gbib/lastUpdate
LOGFILE=/var/log/gbibUpdates.log
DEBUG=0
if [[ "$#" -ne 0 ]]; then
    DEBUG=1
fi

# function to echo only if run with some arguments
function echoDebug {
   if [[ DEBUG -eq "1" ]]; then
     echo $*
   fi
} 

# make sure that apt-get never opens an interactive dialog
export DEBIAN_FRONTEND=noninteractive 

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
    echoDebug GBiB update already running, not starting
    exit 2
fi
	
# if run with no argument (=from cron): check if the VirtualBox guest addition
# kernel modules work and if yes, if auto-updates were deactivated from the
# Vbox host via a property
if [ "$#" -eq 0 ] ; then
    if modprobe vboxguest 2> /dev/null > /dev/null; then
       if VBoxControl guestproperty get gbibAutoUpdateOff | grep -xq "Value: yes" 2> /dev/null ; then
           exit 6
       fi
    fi
# show a little note when VirtualBox kernel module is not working and we're not running under cron
else
    if [ ! modprobe vboxguest 2> /dev/null > /dev/null ] ; then
      echo - Info: GBiB not running on VirtualBox or VirtualBox Guest Utils are not working
    fi
fi


# check if we have internet, stop if not
wget -q --tries=1 --timeout=10 --spider http://hgdownload.soe.ucsc.edu -O /dev/null
if [ $? -ne 0 ] ; then
    echoDebug GBiB has no connection to hgdownload.soe.ucsc.edu, cannot update now
    exit 3
fi

# check flag file if run with no parameter (=from cron)
if [ "$#" -eq 0 ] ; then
   # check a flag file to see if anything on hgdownload actually changed
   if /root/urlIsNotNewerThanFile $UPDATEFLAG /root/lastUpdateTime.flag
   then
       exit 0
   fi
fi

# unless already calling self, update self and call self unless doing only cgis
# self-updates are not done when suppressed with notSelf and also not in hgwdev-mode to allow testing of local updateBrowser.sh changes
# Internal sidenote: if you want hgwdev CGIs and also the current hgwdev update
# script, do a gbibCoreUpdateBeta+updateBrowser hgwdev

# gbibCoreUpdateBeta ends with -Beta because it is used during beta time, to
# test the current dev update script The update script itself has only a
# two-stage release process, beta and final, as the alpha version of the script
# is on the GBiB of the developer itself.
# the file /root/gbibSkipUpdate allows to skip one single auto-update
if [[ ( "$BASH_ARGV" != "notSelf" && "$1" != "hgwdev" ) && ( ! -e /root/gbibSkipNextUpdate ) ]] ; then
    echo getting new update script
    # we got three VMs where updateBrowser.sh was 0 bytes, so doing download/move now
    wget http://hgdownload.soe.ucsc.edu/gbib/updateBrowser.sh -O /root/updateBrowser.sh.new -q && mv /root/updateBrowser.sh.new /root/updateBrowser.sh
    chmod a+x /root/updateBrowser.sh
    /root/updateBrowser.sh $1 notSelf
    exit 0
fi

rm -f /root/gbibSkipNextUpdate

# check if any hgMirror jobs are running right now
# check if the group id file exists and also if any processes exist with this group id
# note that the .pid actually contains a group id, not a process id
if [ -f /tmp/lastJob.pid ] && [ "$(ps x -o pgid | grep $(cat /tmp/lastJob.pid) | wc -l)" != "0" ] ; then
    echo a hgMirror job is running right now, not updating
    exit 4
fi
	
# --- now do the update ---

# keep a log of all output of this script and the date
echo --------------------------------- >> $LOGFILE
date  >> $LOGFILE
echo --------------------------------- >> $LOGFILE
exec >> >(tee -a $LOGFILE) 2>&1

# not done, as old customTrash tables will be in innoDb format
# and can't be read if we deactivate it now
# deactivate inno-db support in mysql. Saves 400-500MB of RAM.
#if ! grep skip-innodb /etc/mysql/my.cnf > /dev/null 2> /dev/null ; then
    #echo - Switching off inno-db in /etc/mysql/my.cnf
    #sed -i '/^.mysqld.$/a skip-innodb' /etc/mysql/my.cnf
    #sed -i '/^.mysqld.$/a default-storage-engine=myisam' /etc/mysql/my.cnf
    #service mysql restart
#fi
   
# activate the apt repo 'main' and 'universe' so we can install external software
if ! apt-cache policy r-base | grep "Unable to locate" > /dev/null; then
   if ! grep '^deb http://us.archive.ubuntu.com/ubuntu trusty main universe multiverse$' /etc/apt/sources.list > /dev/null; then
       echo - Activating the main Ubuntu package repository
       echo 'deb http://us.archive.ubuntu.com/ubuntu trusty main universe multiverse' >> /etc/apt/sources.list
   fi
fi

# activate daily automated security updates with automated reboots
# automated reboots are strange but probably better than to risk exploits
# see https://help.ubuntu.com/community/AutomaticSecurityUpdates
if apt-cache policy unattended-upgrades | grep "Installed: .none." > /dev/null; then
   echo - Activating automated daily Ubuntu security updates
   # from http://askubuntu.com/questions/203337/enabling-unattended-upgrades-from-a-shell-script
   apt-get update
   apt-get install -y unattended-upgrades update-notifier-common
   # update package lists every day
   echo 'APT::Periodic::Update-Package-Lists "1";' > /etc/apt/apt.conf.d/20auto-upgrades
   # do the upgrade every day
   echo 'APT::Periodic::Unattended-Upgrade "1";' >> /etc/apt/apt.conf.d/20auto-upgrades
   # reboot if needed
   echo 'Unattended-Upgrade::Automatic-Reboot "true";' >> /etc/apt/apt.conf.d/20auto-upgrades
   # remove packages not used anymore
   echo 'Unattended-Upgrade::Remove-Unused-Dependencies "true";' >> /etc/apt/apt.conf.d/20auto-upgrades
   # and remove the downloaded tarballs at the end
   echo 'APT::Periodic::AutocleanInterval "1";' >> /etc/apt/apt.conf.d/20auto-upgrades
   /etc/init.d/unattended-upgrades restart
fi
   
# unattended security upgrades take a while to start, better to force one right now and show the progress
# this may lead to an auto-reboot, so let's skip the auto-update of this script for this reboot
touch /root/gbibSkipNextUpdate
echo - Applying Ubuntu security updates
unattended-upgrade -v
rm -f /root/gbibSkipNextUpdate

# The original GBiB image did not use the Ubuntu Virtualbox guest utils but the
# ones from VirtualBox in /opt. Fix this now and switch to the Ubuntu guest
# utilities, so they are updated automatically by the Ubuntu tools
if apt-cache policy virtualbox-guest-dkms | grep "Installed: .none." > /dev/null; then
    echo - Updating VirtualBox Guest utilities
    apt-get install -y linux-headers-generic 
    apt-get install -y virtualbox-guest-dkms
    apt-get -y autoremove
    /etc/init.d/vboxadd start

fi

# install R for the gtex tracks
if apt-cache policy r-base-core | grep "Installed: .none." > /dev/null; then
   echo - Installing R
   apt-get update
   apt-get --no-install-recommends install -y r-base-core
   apt-get -y autoremove
fi

# install imagemagick for the session gallery
if apt-cache policy imagemagick | grep "Installed: .none." > /dev/null; then
   echo - Installing imagemagick
   apt-get update
   apt-get --no-install-recommends install -y imagemagick
   apt-get -y autoremove
fi

# install mysql-python for hgGeneGraph, actually cgi-bin/pyLib/hgLib.py
if apt-cache policy python-mysqldb | grep "Installed: .none." > /dev/null; then
   echo - Installing mysql-python
   apt-get update
   apt-get --no-install-recommends install -y python-mysqldb
   apt-get -y autoremove
fi

# install curl for checking if we are closer to European or US mirror
if apt-cache policy curl | grep "Installed: .none." > /dev/null; then
   echo - Installing curl
   apt-get update
   apt-get --no-install-recommends install -y curl
   apt-get -y autoremove
   # make sure that the mysql server is re-configured now
   rm -f /usr/local/apache/trash/registration.txt
fi

echo
echo - Updating the genome browser software via rsync:

# CGI-BIN and HTDOCS:
# the parameter "hgwdev" copies over only the beta/alpha CGIs from hgwdev
if [ "$1" == "hgwdev" ] ; then
    # note the missing -u option to RSYNC: in hgwdev mode, we want to overwrite everything.
    # On a development machine, the developer might have touched a file
    # for testing. We want to make sure that all local files are overwritten by the 
    # files on hgwdev
    RSYNCOPTS="-ltrvh"
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
      RSYNCAPACHE="$RSYNCAPACHE --exclude ENCODE/**.pdf --exclude *.gz --exclude *.bw --exclude *.bb --exclude *.bam --exclude goldenPath/**.pdf --exclude admin/** --exclude goldenPath/customTracks/** --exclude pubs/** --exclude ancestors/** --exclude training/** --exclude trash --exclude style-public/** --exclude js-public/** --exclude **/edw* --exclude images/mammalPsg/** --exclude **/encodeTestHub* --exclude favicon.ico --exclude folders --exclude ENCODE/** --exclude ENCODE/** --exclude Neandertal/** --exclude gbib/** --exclude generator/** --exclude js-*/** --exclude js/*/* --exclude .\* --exclude x86_64/* --exclude .xauth --exclude .hg.conf --exclude hgHeatmap* --exclude hg.conf --exclude 'hgt/**' --exclude admin/** --exclude images --exclude trash --exclude edw* --exclude visiGeneData/** --exclude crom_dir/ --exclude testp/ --exclude *.exe --exclude *.old --exclude *.tmp --exclude *.bak --exclude test* --exclude hg.conf* --exclude **/hgHeatmap* --exclude ~* --exclude Intronerator** --exclude hgText --exclude hgSubj --exclude gisaid* --exclude nt4.dir --exclude qaPush* --exclude docIdView --exclude ct/ --exclude *.bak --exclude hg.conf* --exclude gsid*/ --exclude *.private --exclude useCount --exclude ~* --exclude lssnp/ --exclude hg.conf.local"
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
    echo - Updating CGIs...
    rsync --delete -u $RSYNCOPTS $RSYNCSRC/$RSYNCCGIBIN /usr/local/apache/cgi-bin/ --exclude=hg.conf --exclude=hg.conf.local --exclude edw* --exclude *private --exclude hgNearData --exclude visiGeneData --exclude Neandertal 

    echo - Updating HTML files...
    # not using -u because we had a case with a 0-byte html page that was 
    # not updated anymore in #18337
    rsync --delete $RSYNCOPTS $RSYNCSRC/$RSYNCHTDOCS/ /usr/local/apache/htdocs/ --include **/customTracks/*.html --exclude ENCODE/ --exclude *.bam --exclude *.bb --exclude */*.bw --exclude */*.gz --exclude favicon.ico --exclude folders --exclude ancestors --exclude admin --exclude goldenPath/customTracks --exclude images/mammalPsg --exclude style/gbib.css --exclude images/title.jpg --exclude images/homeIconSprite.png --exclude goldenPath/**.pdf --exclude training

    PUSHLOC=hgdownload.cse.ucsc.edu::gbib/push/
fi

chown -R www-data.www-data /usr/local/apache/cgi-bin/*
chown -R www-data.www-data /usr/local/apache/htdocs/
chmod -R a+r /usr/local/apache/htdocs

# June 2017: add a basic set of hg38 files to the GBIB
# This will add 1.2 GB to the size of the virtual disc but hg38
# is the default genome so probably should be included
# Touching the files once is enough. Rsync will download them.
mkdir -p /data/gbdb/hg38
mkdir -p /data/gbdb/hg38/targetDb/
mkdir -p /data/gbdb/hg38/html/
mkdir -p /data/mysql/hg38

# GBDB files
for i in hg38.2bit html/description.html knownGene.ix knownGene.ixx knownGene.bb targetDb/kgTargetSeq10.2bit targetDb/kgTargetSeq8.2bit targetDb/kgTargetSeq9.2bit trackDb.ix trackDb.ixx; do
   touch /data/gbdb/hg38/$i;
done

# MySQL tables
for i in chromInfo cytoBand cytoBandIdeo ensemblLift extFile grp gtexGene gtexGeneModel hgFindSpec kgColor kgXref knownCanonical knownGene knownToTag ncbiRefSeq ncbiRefSeqCurated ncbiRefSeqLink ncbiRefSeqOther ncbiRefSeqPredicted ncbiRefSeqPsl refGene tableList trackDb ucscToEnsembl ucscToINSDC xenoRefGene; do
   touch /data/mysql/hg38/$i.MYD;
   touch /data/mysql/hg38/$i.MYI;
   touch /data/mysql/hg38/$i.frm;
done

# adding tables that are required for gtex, which is now a default track, #19587
touch /data/mysql/hgFixed/gtexInfo.{MYI,MYD,frm}
touch /data/mysql/hgFixed/gtexTissue.{MYI,MYD,frm}

# -- END JUNE 2017

if [ "$1" != "hgwdev" ] ; then
  echo - Updating GBDB files...
  rsync $RSYNCOPTS --existing rsync://hgdownload.cse.ucsc.edu/gbdb/ /data/gbdb/
  chown -R www-data.www-data /data/gbdb/
fi

echo - Pulling other files
# make sure we never overwrite the hg.conf.local file
rsync $RSYNCOPTS $PUSHLOC / --exclude=hg.conf.local

# July 2016: add the cram fetcher to root's crontab 
# this has to be done after the PUSHLOC directory has been copied over
if grep -q fetchCramReference /var/spool/cron/crontabs/root; then
        true
   else
       crontab -l | awk '{print} END {print "\n# handle CRAM auto reference download\n*/1 * * * * /root/fetchCramReference.sh /data/cramCache/pending /data/cramCache/ /data/cramCache/error/\n"}' | crontab -   
fi
# also create the directories for the cram files
# cram also requires that the directories are writable by the apache user
if [ ! -d /data/cramCache ]; then
    mkdir -p /data/cramCache/pending /data/cramCache/error
    chown -R www-data:www-data /data/cramCache
fi
# -- end July 2016

# July 2016: genbank tables are now in hgFixed. By touching a few files, we make sure that at least refseqStatus 
# is rsync'ed later, otherwise gbib is really slow, refs #17842
touch /data/mysql/hgFixed/refSeqStatus.MYI /data/mysql/hgFixed/refSeqStatus.MYD /data/mysql/hgFixed/refSeqStatus.frm 
touch /data/mysql/hgFixed/refLink.MYI /data/mysql/hgFixed/refLink.MYD /data/mysql/hgFixed/refLink.frm 

# Jan 2017: hgVai does not work if /data/mysql/hg19/wgEncodeRegTfbsClusteredInputsV3 is not present, so force this 
# table into the rsync, refs #18778
touch /data/mysql/hg19/wgEncodeRegTfbsClusteredInputsV3.{frm,MYI,MYD}

# Jun 2017: An Ubuntu security update in early 2017 deactivated LOAD DATA in Mysql
# so we are switching it back on
if grep -q secure-file-priv /etc/mysql/my.cnf; then
    true
else
    echo Allowing LOAD DATA in MySQL and restart MySQL
    sed -i '/\[mysqld\]/a secure-file-priv = ""' /etc/mysql/my.cnf
    service mysql restart
fi

# we can now remove the old tables
rm -f /data/mysql/hg19/refSeqStatus*

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

echo - Adapting the menu 
cp /usr/local/apache/htdocs/inc/globalNavBar.inc /tmp/navbar.inc
# remove mirrors and downloads menu
sed -i '/<li class="menuparent" id="mirrors">/,/^<\/li>$/d' /tmp/navbar.inc 
sed -i '/<li class="menuparent" id="downloads">/,/^<\/li>$/d' /tmp/navbar.inc 
# adding the link to the mirror tracks tool
sed -i '/hgLiftOver/a <li><a href="../cgi-bin/hgMirror">Mirror tracks</a></li>' /tmp/navbar.inc
# add a link to the gbib shared data folder
sed -i '/Track Hubs/a <li><a target="_blank" href="http:\/\/127.0.0.1:1234\/folders\/">GBiB Shared Data Folder<\/a><\/li>' /tmp/navbar.inc
# adding a link to the GBIB help pages
sed -i '/genomewiki/a <li><a href="../goldenPath/help/gbib.html">Help on GBiB</a></li>' /tmp/navbar.inc
cat /tmp/navbar.inc | grep -v hgNear | grep -v hgVisiGene | uniq > /usr/local/apache/htdocs/inc/globalNavBar.inc
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

# the local-only hg.conf settings file has to exist as it is included from hg.conf
# In case it got deleted due to some error, recreate it
if [ ! -f /usr/local/apache/cgi-bin/hg.conf.local ] ; then
   echo Creating /usr/local/apache/cgi-bin/hg.conf.local
   echo allowHgMirror=true > /usr/local/apache/cgi-bin/hg.conf.local
fi

# Sept 2017: check if genome-euro mysql server is closer
if [ ! -f /usr/local/apache/trash/registration.txt ]; then
   curl -sSI genome-euro.ucsc.edu 2>&1 > /dev/null
   if [[ $? -eq 0 ]]; then
      echo comparing latency: genome.ucsc.edu Vs. genome-euro.ucsc.edu
      euroSpeed=$( (time -p (for i in `seq 10`; do curl -sSI genome-euro.ucsc.edu > /dev/null; done )) 2>&1 | grep real | cut -d' ' -f2 )
      ucscSpeed=$( (time -p (for i in `seq 10`; do curl -sSI genome.ucsc.edu > /dev/null; done )) 2>&1 | grep real | cut -d' ' -f2 )
      if [[ $(awk '{if ($1 <= $2) print 1;}' <<< "$euroSpeed $ucscSpeed") -eq 1 ]]; then
         echo genome-euro seems to be closer
         echo modifying gbib to pull data from genome-euro instead of genome
         sed -i s/slow-db.host=genome-mysql.cse.ucsc.edu/slow-db.host=genome-euro-mysql.soe.ucsc.edu/ /usr/local/apache/cgi-bin/hg.conf
      else
         echo genome.ucsc.edu seems to be closer
         echo not modifying /usr/local/apache/cgi-bin/hg.conf
      fi
   fi
fi

touch /root/lastUpdateTime.flag
echo - GBiB update done
cat /etc/issue | tr -s '\n'
