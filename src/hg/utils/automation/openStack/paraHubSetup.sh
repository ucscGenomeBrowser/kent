#!/bin/bash

# this script ends up in:
#	/var/lib/cloud/instance/scripts/part-001
# running as the 'root' user and is run during first start up of the machine

# start a log file to record some information from the operation of this script:
export logFile="/tmp/startUpScript.log.$$"

# record environment to find out the operating conditions of this script

printf "#### begin start up script log %s\n" "`date '+%s %F %T'`" > "${logFile}"
printf "#### uname -a\n" >> "${logFile}"
uname -a >> "${logFile}" 2>&1
printf "#### df -h\n" >> "${logFile}"
df -h >> "${logFile}" 2>&1
printf "#### env\n" >> "${logFile}"
env >> "${logFile}" 2>&1
printf "#### set\n" >> "${logFile}"
set >> "${logFile}" 2>&1
printf "#### arp -a\n" >> "${logFile}"
arp -a >> "${logFile}" 2>&1
printf "#### ifconfig -a\n" >> "${logFile}"
ifconfig -a >> "${logFile}" 2>&1
printf "#### PATH\n" >> "${logFile}"
printf "${PATH}\n" >> "${logFile}" 2>&1

if [ -s /home/centos/.bashrc ]; then
  printf "## parasol hub machine started install %s\n" "`date '+%s %F %T'`" >> /etc/motd
  mkdir -p /data/bin /data/scripts /data/genomes /home/centos/bin /data/parasol/nodeInfo
  chmod 777 /data
  chmod 755 /data/bin /data/scripts /data/genomes /data/parasol /home/centos/bin
  chmod 755 /home/centos

  # these additions to .bashrc should protect themselves from repeating
  printf "export PATH=/data/bin:/data/scripts:/home/centos/bin:\$PATH\n" >> /home/centos/.bashrc
  printf "export LANG=C\n" >> /home/centos/.bashrc
  printf "alias og='ls -ogrt'\n" >> /home/centos/.bashrc
  printf "alias plb='parasol list batches'\n" >> /home/centos/.bashrc
  printf "alias vi='vim'\n" >> /home/centos/.bashrc
  printf "set -o vi\n" >> /home/centos/.bashrc
  printf "set background=dark\n" >> /home/centos/.vimrc
  export subNet=`ifconfig -a | grep broadcast | head -1 | awk '{print $2}' | awk -F'.' '{printf "%s.%s.%s.0", $1,$2,$3}'`
  printf "/data    ${subNet}/16(rw)\n" > /etc/exports

  # install the wget command right away so the wgets can get done
  yum -y install wget >> "${logFile}" 2>&1
  rsync -a rsync://hgdownload.soe.ucsc.edu/genome/admin/exe/linux.x86_64/ /data/bin/
  # initParasol script to manage start/stop of parasol system
  wget -qO /data/parasol/initParasol 'http://genomewiki.ucsc.edu/images/4/4f/InitParasol.sh.txt' >> "${logFile}" 2>&1
  chmod 755 /data/parasol/initParasol
  # script in nodeInfo to be used by the nodes to report themselves
  wget -qO /data/parasol/nodeInfo/nodeReport.sh 'http://genomewiki.ucsc.edu/images/e/e3/NodeReport.sh.txt' >> "${logFile}" 2>&1
  chmod 755 /data/parasol/nodeInfo/nodeReport.sh
  # bedSingleCover.pl for use in running featureBits like measurements
  wget -O /data/scripts/bedSingleCover.pl 'http://genome-source.cse.ucsc.edu/gitweb/?p=kent.git;a=blob_plain;f=src/utils/bedSingleCover.pl' >> "${logFile}" 2>&1
  chmod +x /data/scripts/bedSingleCover.pl
  chown -R centos:centos /data /home/centos/bin /home/centos/.vimrc

  # and now can start the rest of yum installs, these take a while
  # useful to have the 'host' command, 'traceroute' and nmap ('nc')
  # to investigate the network, wget for transfers, and git for kent source tree
  # the git-all installs 87 packages, including perl, tcsh for the csh shell,
  # screen for terminal management, vim for editing convenience, bc for math
  yum -y install bind-utils traceroute nmap-ncat tcsh screen vim-X11 vim-common vim-enhanced vim-minimal git-all bc >> "${logFile}" 2>&1

  # these systemctl commands may not be necessary
  # the package installs may have already performed these initalizations
  systemctl enable rpcbind >> "${logFile}" 2>&1
  systemctl enable nfs-server >> "${logFile}" 2>&1
  systemctl start rpcbind >> "${logFile}" 2>&1
  systemctl start nfs-server >> "${logFile}" 2>&1
  systemctl start nfs-lock >> "${logFile}" 2>&1
  systemctl start nfs-idmap >> "${logFile}" 2>&1
  exportfs -a >> "${logFile}" 2>&1

  # this business needs to be improved to be a single rsync from
  # a download directory.
  git archive --format=tar --remote=git://genome-source.soe.ucsc.edu/kent.git \
      --prefix=kent/ HEAD src/hg/utils/automation \
        | tar vxf - -C /data/scripts --strip-components=5 \
      --exclude='kent/src/hg/utils/automation/incidentDb' \
      --exclude='kent/src/hg/utils/automation/configFiles' \
      --exclude='kent/src/hg/utils/automation/ensGene' \
      --exclude='kent/src/hg/utils/automation/genbank' \
      --exclude='kent/src/hg/utils/automation/lastz_D' \
      --exclude='kent/src/hg/utils/automation/openStack' >> "${logFile}" 2>&1

  chown -R centos:centos /data/scripts

  printf "## parasol hub machine finished install %s\n" "`date '+%s %F %T'`" >> /etc/motd
  date '+%s %F %T' > /tmp/hub.machine.ready.signal
fi
printf "#### end start up script log %s\n" "`date '+%s %F %T'`" >> "${logFile}"
