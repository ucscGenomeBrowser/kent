#!/bin/sh
# this will delete all temporary data on the box
# e.g. all custom tracks, user session data etc
# and all temp files

# clean docs
find /usr/share/doc -depth -type f | xargs rm -f || true
rm -rf /usr/share/man /usr/share/groff /usr/share/info /usr/share/lintian 

# clean package repo
rm -rf /var/lib/apt/lists/* 
apt-get clean

# clean log files
> /var/log/messages
> /var/log/auth.log
> /var/log/kern.log
> /var/log/bootstrap.log
> /var/log/dpkg.log
> /var/log/syslog
> /var/log/daemon.log
rm -f /var/tmp/*
rm -rf /tmp/*
rm -f /var/log/*.gz
rm -f /var/log/*.1 /var/log/*.0
rm -rf /var/log/apache2/*
rm -f /var/log/upstart/*.gz
rm -f /var/log/mysql/*.gz

# clean trash
cd /data/trash/
rm -rf udcCache/*
rm -rf hgc/*
# a dangerous command to delete all trash?
rm -f `find /data/trash/ -type f` 
rm -f /usr/local/apache/userdata/cleaner.pid 
rm -f /var/tmp/*

# clean session info
mysql hgcentral -e 'truncate sessionDb'
mysql hgcentral -e 'truncate namedSessionDb'
mysql hgcentral -e 'truncate userDb'
mysql hgcentral -e 'truncate hubStatus'
mysql hgcentral -e 'delete from gbMembers where userName<>"browser"'

rm /var/spool/mail/browser

# clean swap
echo zapping swapfile
swapoff /swapfile
dd if=/dev/zero of=/swapfile bs=1M count=1024
chmod 600 /swapfile
mkswap /swapfile

# reset update time, make sure that the box will update itself on next restart ?
# rm /root/lastUpdateTime.flag
