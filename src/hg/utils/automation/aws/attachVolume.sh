#!/bin/bash
#
# attachVolume.sh - given a Volume identifier, attach that volume to this
#   running instance.  This script is used by the 'startParaHub.sh' script
#   as it is starting up this machine.  The ec2-user account will have
#   the AWS access key and secret access key set as environment variables
#   in the .bashrc file.
#
# provide one argument to this command: the volume identifier
# e.g.:
#  ./bin/attachVolume.sh vol-0123456789abcdef0
#
if [ $# -ne 1 ]; then
  printf "usage: attachVolume.sh <volume Id>\n" 1>&2
  exit 255
fi

# given volume Id to use:
export volumeId=$1

# check if already mounted, exit if true, done
if [ -d "/genomes" ]; then
  mountpoint /genomes > /dev/null && exit 0
fi

# not mounted, pick up volume identifier from instance tags
export instId=`curl http://169.254.169.254/latest/meta-data/instance-id 2> /dev/null`
# dataVolumeId should be in the tag identified by 'volumeId'
export dataVolumeId=`aws ec2 describe-tags --filters "Name=resource-id,Values=$instId" | grep -w volumeId | awk '{print $NF}'`
export strLength=${#dataVolumeId}
if [ "$strLength" -lt 10 ]; then
  printf "ERROR: cannot find 'volumeId' tag in this instance: $instId\n" 1>&2
  printf "The instance should have a 'volumeId' tag to specify volume to attach\n" 1>&2
  exit 255
fi
if [ "${volumeId}" != "${dataVolumeId}" ]; then
  printf "ERROR: given volumeId:\n\t'$volumeId' does not match 'volumeId' tag:\n\t'${dataVolumeId}' in instance: "${instId}"\n" 1>&2
  exit 255
fi
# see if the volume is attached
export xvdbPresent=`sudo file -s /dev/xvdb | grep "cannot open" | wc -l`
if [ "${xvdbPresent}" -eq 1 ]; then
  # attach specified volume
  aws ec2 attach-volume --volume-id "${dataVolumeId}" \
     --instance-id "${instId}" --device "/dev/sdb"
  sleep 5
fi
# it should be present now
xvdbPresent=`sudo file -s /dev/xvdb | grep "cannot open" | wc -l`
if [ "${xvdbPresent}" -eq 1 ]; then
  printf "ERROR: attach volume $dataVolumeId has failed ?" 1>&2
  printf "to this instance: $instId\n" 1>&2
  exit 255
fi
# see if the filesystem has already been formatted
export validFs=`sudo file -s /dev/xvdb | grep "ext4 filesystem data" | wc -l`
if [ "${validFs}" -ne 1 ]; then
  sudo mkfs -t ext4 /dev/xvdb
  sleep 5
fi
# it should be formatted now:
validFs=`sudo file -s /dev/xvdb | grep "ext4 filesystem data" | wc -l`
if [ "${validFs}" -ne 1 ]; then
  printf "ERROR: attached volume $dataVolumeId is not formatted\n" 1>&2
  printf "\tas expected 'ext4' filesystem ?\n" 1>&2
  exit 255
fi
# the filesystem can now be mounted, see if it is already in /etc/fstab
export xvdbUuid=`ls -al /dev/disk/by-uuid | grep xvdb | awk '{print $9}'`
export expLength=20
strLength=${#xvdbUuid}
if [ "$strLength" -le $expLength ]; then
  printf "ERROR: can not find UUID of disk /dev/xvdb ?\n" 1>&2
  printf "Expecting to find disk xvdb in list of directory /dev/disk/by-uuid\n" 1>&2
  exit 255
fi

# this 'grep || sudo' will place the line in /etc/fstab if it is not there
grep "^UUID=${xvdbUuid}" /etc/fstab > /dev/null 2>&1 || sudo printf "UUID=%s\t/genomes\text4\tdefaults,nofail\t0\t2\n" "${xvdbUuid}" | sudo tee -a /etc/fstab > /dev/null
# verify it is really there
grep "^UUID=${xvdbUuid}" /etc/fstab > /dev/null 2>&1
if [ $? -ne 0 ]; then
  printf "ERROR: can not find UUIC=$xvdbUuid in /etc/fstab\n" 1>&2
fi

# ready to mount
sudo mkdir -p /genomes
sudo chmod 777 /genomes
sudo chown ec2-user:ec2-user /genomes
sudo mount /genomes
sudo chmod 777 /genomes
sudo chown ec2-user:ec2-user /genomes

# check if already mounted, exit if true, done
mountpoint /genomes > /dev/null && exit 0
printf "ERROR: attaching, formatting, mounting of data volume has failed\n" 1>&2
printf "for specified volume: $dataVolumeId on instance: $instId\n" 1>&2

exit 255
