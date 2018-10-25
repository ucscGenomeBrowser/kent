#!/bin/bash

# on AWS and OpenStack, This script ends up in:
#       /var/lib/cloud/instance/scripts/part-001
# running as the 'root' user and is run during first start up of the machine
# This script attempts to keep its own log file.  If anything leaks out from
#   here, it will end up in:
#    /var/log/cloud-init-output.log

# start a log file to record information from the operation of this script:
export logFile="/tmp/startUpScript.log.$$"

# record environment to find out the operating conditions of this script

printf "#### begin start up script log %s\n" "`date '+%s %F %T'`" > "${logFile}"
printf "#### uname -a\n" >> "${logFile}"
uname -a >> "${logFile}" 2>&1
## printf "#### df -h\n" >> "${logFile}"
## df -h >> "${logFile}" 2>&1
printf "#### env\n" >> "${logFile}"
env >> "${logFile}" 2>&1
printf "#### set\n" >> "${logFile}"
set >> "${logFile}" 2>&1
printf "#### arp -a\n" >> "${logFile}"
arp -a >> "${logFile}" 2>&1
printf "#### ifconfig -a\n" >> "${logFile}"
ifconfig -a >> "${logFile}" 2>&1
printf "#### /etc/fstab\n" >> "${logFile}"
cat /etc/fstab >> "${logFile}"
printf "#### PATH\n" >> "${logFile}"
printf "${PATH}\n" >> "${logFile}" 2>&1

export homeDir="notFound"
export nativeUser="notFound"

# determine OpenStack or AWS environment
if [ -d "/home/centos" ]; then
  nativeUser="centos"
elif [ -d "/home/ec2-user" ]; then
  nativeUser="ec2-user"
fi
homeDir="/home/$nativeUser"

if [ -s "${homeDir}/.bashrc" ]; then
  export startTime=`date "+%s"`
  printf "## parasol hub machine started install %s\n" "`date '+%s %F %T'`" >> /etc/motd
  export instanceId=`curl http://169.254.169.254/latest/meta-data/instance-id`
  # directories and symlinks to make this machine appear very much as it
  # would in the U.C. Santa Cruz Genome browser development environment
  mkdir -p /data/bin /data/scripts /data/genomes /data/bedtools /data/parasol/nodeInfo ${homeDir}/bin /hive /cluster/bin /scratch /genomes
  chmod 777 /data /scratch /hive /cluster /cluster/bin /genomes
  chmod 755 /data/bin /data/scripts /data/genomes /data/parasol ${homeDir}/bin
  chmod 755 ${homeDir}
  ln -s /data/bedtools /cluster/bin/bedtools
  ln -s /data /hive/data
  ln -s /dev/shm /scratch/tmp

  # these additions to .bashrc should protect themselves from repeating
  printf "export PATH=/data/bin:/data/scripts:${homeDir}/bin:\$PATH\n" >> "${homeDir}/.bashrc"
  printf "export LANG=C\n" >> "${homeDir}/.bashrc"
  printf "alias og='ls -ogrt'\n" >> "${homeDir}/.bashrc"
  printf "alias plb='parasol list batches'\n" >> "${homeDir}/.bashrc"
  printf "alias vi='vim'\n" >> "${homeDir}/.bashrc"
  printf "set -o vi\n" >> "${homeDir}/.bashrc"
  printf "export AWS_DEFAULT_OUTPUT=text\n" >> "${homeDir}/.bashrc"
  if [ "${nativeUser}" = "ec2-user" ]; then
    availZone=`ec2-metadata --availability-zone | sed -e 's/placement: *//;'`
    region=`echo ${availZone:0:${#availZone} - 1}`
    printf "export AWS_DEFAULT_REGION=%s\n" "${region}" >> "${homeDir}/.bashrc"
    printf "export AWS_ACCESS_KEY_ID=AWS_AccessKey\n" >> "${homeDir}/.bashrc"
    printf "export AWS_SECRET_ACCESS_KEY=AWS_SecretKey\n" >> "${homeDir}/.bashrc"
    printf "instId: %s, availZone: %s, region: %s\n" "${instanceId}" "${availZone}" "${region}" >> "${logFile}"
  fi
  printf "set background=dark\n" >> "${homeDir}/.vimrc"

  # install the wget command right away so the wgets can get done
  yum -y install wget >> "${logFile}" 2>&1
  # and the kent command line utilities into /data/bin/
  rsync -a --stats rsync://hgdownload.soe.ucsc.edu/genome/admin/exe/linux.x86_64/ /data/bin/ >> "${logFile}" 2>&1
  # initParasol script to manage start/stop of parasol system
  wget -qO /data/parasol/initParasol 'http://genomewiki.ucsc.edu/images/4/4f/InitParasol.sh.txt' >> "${logFile}" 2>&1
  chmod 755 /data/parasol/initParasol
  # script in nodeInfo to be used by the nodes to report themselves
  wget -qO /data/parasol/nodeInfo/nodeReport.sh 'http://genomewiki.ucsc.edu/images/e/e3/NodeReport.sh.txt' >> "${logFile}" 2>&1
  chmod 755 /data/parasol/nodeInfo/nodeReport.sh
  # bedSingleCover.pl for use in running featureBits like measurements
  wget -P /data/scripts 'https://genome-source.gi.ucsc.edu/gitlist/kent.git/raw/master/src/utils/bedSingleCover.pl' >> "${logFile}" 2>&1
  chmod +x /data/scripts/bedSingleCover.pl
  chown -R ${nativeUser}:${nativeUser} /data "${homeDir}/bin" "${homeDir}/.vimrc"

  # ec2-user HOME/bin/attachVolume.sh used to attach extra data volume
  wget -qO "${homeDir}/bin/attachVolume.sh"  'http://genomewiki.ucsc.edu/images/2/2c/AwsAttachVolume.sh.txt' >> "${logFile}" 2>&1
  chmod 755 "${homeDir}/bin/attachVolume.sh"
  chown ${nativeUser}:${nativeUser} "${homeDir}/bin/attachVolume.sh"
  chown ${nativeUser}:${nativeUser} "/genomes"

  # and now can start the rest of yum installs, these take a while
  # useful to have the 'host' command, 'traceroute' and nmap ('nc')
  # to investigate the network, wget for transfers, and git for kent source tree
  # the git-all installs 87 packages, including perl, tcsh for the csh shell,
  # screen for terminal management, vim for editing convenience, bc for math
  yum -y update >> "${logFile}" 2>&1
  yum repolist >> "${logFile}" 2>&1
  yum -y install bind-utils traceroute nmap-ncat tcsh screen vim-X11 vim-common vim-enhanced vim-minimal git-all bc >> "${logFile}" 2>&1
  yum -y install epel-release >> "${logFile}" 2>&1
  yum -y install gcc gcc-c++ zlib-devel tkinter libpng12 strace >> "${logFile}" 2>&1
  # this python business allows the crispr calculation pipeline to function
  # need newer python on the OpenStack
  if [ "${nativeUser}" = "centos" ]; then
     yum -y install centos-release-scl >> "${logFile}"
     yum -y install python36 python36-devel >> "${logFile}"
     python3.6 -m ensurepip --default-pip >> "${logFile}"
     python3.6 -m pip install --upgrade pip >> "${logFile}"
     python3.6 -m pip install --ignore-installed pytabix pandas twobitreader scipy matplotlib numpy >> "${logFile}"
     python3.6 -m pip install scikit-learn==0.16.1 Biopython xlwt >> "${logFile}" 2>&1
  else
     yum -y install python-pip python-devel >> "${logFile}" 2>&1
     pip install --ignore-installed pytabix pandas twobitreader scipy matplotlib numpy >> "${logFile}" 2>&1
     pip install scikit-learn==0.16.1 Biopython xlwt >> "${logFile}" 2>&1
  fi
  printf "#### after yum install %s\n" "`date '+%s %F %T'`" >> "${logFile}"

  # these systemctl commands may not be necessary
  # the package installs may have already performed these initalizations
  systemctl enable rpcbind >> "${logFile}" 2>&1
  systemctl enable nfs-server >> "${logFile}" 2>&1
  systemctl start rpcbind >> "${logFile}" 2>&1
  systemctl start nfs-server >> "${logFile}" 2>&1
  systemctl start nfs-lock >> "${logFile}" 2>&1
  systemctl start nfs-idmap >> "${logFile}" 2>&1

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
      --exclude='kent/src/hg/utils/automation/aws' \
      --exclude='kent/src/hg/utils/automation/openStack' >> "${logFile}" 2>&1

  chown -R ${nativeUser}:${nativeUser} /data/scripts >> "${logFile}" 2>&1

  # setup NFS exports to internal LAN
  export privateIp=`curl http://169.254.169.254/latest/meta-data/local-ipv4`
  export subNet=`echo $privateIp | awk -F'.' '{printf "%s.%s.0.0", $1,$2}'`

  printf "# privateIp: '%s', instanceId: '%s', subNet: '%s'\n" "${privateIp}" "${instanceId}" "${subNet}" >> "${logFile}" 2>&1
  printf "# lsblk:\n" >> "${logFile}" 2>&1
  lsblk >> "${logFile}" 2>&1
  printf "# fdisk -l:\n" >> "${logFile}" 2>&1
  fdisk -l >> "${logFile}" 2>&1

  # special case OpenStack /mnt setups, these machines come with 2 Tb
  # already configured in /mnt/ - plenty of disk space for just about
  # any type of work.  Rework fstab so that 2 Tb is mounted on /genomes
  # instead of /mnt/
  if [ "${nativeUser}" = "centos" ]; then
    umount /mnt
    sed --in-place=.bak -e 's@^/dev/vdb@# /dev/vdb@;' /etc/fstab
    printf "/dev/vdb1\t/genomes\txfs\tdefaults,user,nofail\t0\t2\n" >> /etc/fstab
    mount /genomes
    chmod 777 /genomes
    chown -R ${nativeUser}:${nativeUser} /genomes
  fi
  # export both these filesystems for para node access
  printf "/data    ${subNet}/16(rw)\n" >> /etc/exports
  printf "/genomes    ${subNet}/16(rw)\n" >> /etc/exports

  # turn on the exported filesystems
  exportfs -a >> "${logFile}" 2>&1
  printf "### meta data from curl 169.254.169.254\n" >> "${logFile}" 2>&1

  curl http://169.254.169.254/latest/meta-data/ 2> /dev/null | while read N
do
  V=`curl http://169.254.169.254/latest/meta-data/${N} 2> /dev/null`
  printf "#\t%s\t%s\n" "${N}" "${V}"
done >> "${logFile}" 2>&1

  export endTime=`date "+%s"`
  export et=`echo $endTime $startTime | awk '{printf "%d", $1-$2}'`
  printf "## parasol hub machine finished install %s\n" "`date '+%s %F %T'`" >> /etc/motd
  printf "## elapsed time: %d seconds\n" "${et}" >> /etc/motd
  printf "## elapsed time: %d seconds\n## %s\n" "${et}" "`date '+%s %F %T'`" >> /tmp/hub.machine.ready.signal
  printf "## this parasol hub privateIp: %s\n" "${privateIp}" >> /tmp/hub.machine.ready.signal
  printf "## this parasol hub subnet: %s\n" "${subNet}" >> /tmp/hub.machine.ready.signal
  printf "## external IP address: %s from curl icanhazip.com\n" "`curl icanhazip.com`" >> /tmp/hub.machine.ready.signal
  printf "## external IP address: %s from curl wgetip.com\n" "`curl wgetip.com`" >> /tmp/hub.machine.ready.signal
fi
printf "#### end start up script log %s\n" "`date '+%s %F %T'`" >> "${logFile}"
