The following shell commands are GBiB-specific (they are defined in .bashrc):

general commands:

gbibAutoUpdateOff   = switch off automatic weekly updates
gbibAutoUpdateOn    = reactivate automatic weekly updates
gbibOffline         = switch off remote access to UCSC for tables or files
gbibOnline          = reactivte remote access to UCSC for tables or files
gbibMirrorTracksOff = disable the "Mirror tracks" tool
gbibMirrorTracksOn  = enable the "Mirror tracks" tool
gbibAddTools        = download the UCSC genome command line tools into the /data/tools directory. Requires sudo permissions

advanced commands:

gbibCoreUpdate      = download the most current update script from UCSC now, this is part of an automatic update
gbibFixMysql1       = fix all mysql databases, fast version
gbibFixMysql2       = fix all mysql databases, intensive version
gbibResetNetwork    = reinit the eth0 network interface, in case VirtualBox dropped the network connection
gbibUcscTablesLog   = show the tables that had to be loaded through the network from UCSC 
gbibUcscTablesReset = reset the table counters
gbibUcscGbdbLog     = show the gbdb files that had to be loaded through the network from UCSC 
gbibUcscGbdbReset   = reset the gbdb counters

Quick file system overview:

Apache root is in /usr/local/apache/htdocs. 
Genome Browser CGIs are in /usr/local/apache/cgi-bin
The main Genome Browser config file is /usr/local/apache/cgi-bin/hg.conf
This config file is overwritten by weekly updates, settings that are not temporary
go into /usr/local/apache/cgi-bin/hg.conf.local.

The /data directory holds all genomic or user-related data.
It is the mount point of a second virtual disk, /dev/sdb1 which appears as
gbib-data.vdi in the host file system. The mysql root is /data/mysql.  The genome
browser stores some data in files in a a "gbdb" directory, located under /data/gbdb. 
/data/trash is for medium-term temporary Genome Browser files (e.g. uploads),
/data/userdata for long-term Genome Browser files (e.g. sessions)

A symlink in /usr/local/apache/htdocs/folders points to /media where VirtualBox
auto-mounts host folders.
Auto-update scripts are located in /root and called from root's crontab.
