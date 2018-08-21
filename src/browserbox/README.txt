kent/browserbox/

This directory contains the config files that are needed for the 
Genome Browser in a box. It should be possible to convert
a Ubuntu 14 installation into a Genome Browser in a box by copying all these
files into the root directory as sudo. (Redmine #11957) But we have 
never done this, we have a running VM and don't need to rebuild it.

The most important part of the GBIB setup is the update script, located in
this directory under kent/browserbox/root/updateBrowser. Making changes to GBIB
usually means changing this script as WE DO NOT MODIFY THE GBIB VM, we only
make changes to the update script, so all running GBIBs out there will still
work and they are all in sync.

If you change the copy of the update script in this git repo, please be aware
that it will not automatically land on GBIBs to prevent accidentally breaking
them all. You change updateBrowser, copy it to hgwdev.gi.ucsc.edu/gbib, you
test it on your own GBIB with "updateBrowser hgwdev". If it works, you commit the
file into the git repo and request a push of the file from hgwdev.gi.ucsc.edu/gbib to
hgdownload.gi.ucsc.edu/gbib. This is all explained in the genomewiki page below.

This update script rsyncs the directory hgdownload::gbib/push whenever it updates
into / so hg.conf updates, little binary tools or similar changes can also come
in through the push directory. Again, we do not make changes to the GBIB VM, 
but only through the update script or possibly through the push directory.

PLEASE READ the main gbib doc page and the pages linked from there:
http://genomewiki.ucsc.edu/genecats/index.php/Genome_Browser_in_a_Box_config

---

The following information is only relevant is you're thinking about rebuilding
a GBIB-like system from this directory.

The following symlinks are not in git and have to be set manually :
usr/local/apache/htdocs/folders -> /media
usr/local/apache/trash ->  /data/trash/
usr/local/apache/userdata -> /data/userdata/
var/lib/mysql -> /data/mysql

Change ownerships of /data/gbdb, /data/trash and /data/userdata to www-data and /data/mysql to mysql

This tree here does not include the trash cleaner binaries as they are > 5MB. Ask
Hiram to install the trash cleaner if you need it fixed or changed.

The directory clientInstall/ contains two small scripts that add the entry
"genome.ucsc.local" to the local hosts file. One is written in VisualBasic for
Windows, the other is in AppleScript for OSX. The AppleScript version also
contains a firewall rule that redirects localhost:1234 to localhost:80, the
only way to get a restricted port (<1024) to work with VirtualBox. 
(Redmine #12834)
The Applescript can be packaged into an app with the AppleScript editor. The
result can only be copied around as a zip file due to OSX special file
attributes.

the files packages-ubuntu*.txt are the lists of packages installed on the final
gbib in ubuntu13 and ubuntu14.

Various changes of the box are documented in root/changes.txt

The red box clipart is from http://www.clker.com/clipart-14656.html


