#!/bin/bash

# would like to use this to fail on any error, but there seem to be
#  too many failed commands that are testing things to work around that
# set -beEu -o pipefail

# to use a command like this in a variable, need to use 'eval'
export sshCmd="ssh -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes'"

# the 1>&2 makes the printf output to stderr

if [ $# -ne 3 ]; then
  printf "usage: ./startParaNode.sh <internal IP for paraHub> <id number 0-9> <ssh-key-name>\n" 1>&2
  printf "e.g: ./startParaNode.sh 10.109.0.40 3 keyName\n" 1>&2
  exit 255;
fi

export paraHub=$1
export nodeId=$2
export keyName=$3

printf "# setting up for hub: %s\n" "${paraHub}" 1>&2

# haven't yet figured out how to pass arguments to this script in the
# machine instance.  In the meantime, customize the startup script with
# the arguments we know about here.
sed -e "s/PARAHUB/$paraHub/g;" paraNodeSetup.sh.template > paraNodeSetup.sh

function dividingLine {
  dateTag=`date "+%s %F %T"`
  printf "############  %s ######################\n" "$dateTag"
}

# waiting here for the sshd to respond in the newly started image
function waitSshKeys {
  export userName=$1
  export ip=$2
  export nodeUser="${userName}@${ip}"
  export returnCode=1
  export waitSeconds=5
  while [ "$returnCode" -ne 0 ]
  do
    sleep $waitSeconds
    printf "### waiting ${waitSeconds}s on 'ssh ${nodeUser}'\n" 1>&2
    eval "${sshCmd}" "${nodeUser}" 'cat /tmp/node.machine.ready.signal'
    returnCode=$?
    printf "### returned from 'ssh ${nodeUser}' returnCode: $returnCode\n" 1>&2
    waitSeconds=`echo $waitSeconds | awk '{print $1+1}'`
  done
  printf "### wait on 'ssh ${nodeUser} is complete\n" 1>&2
}

# available images:
# +--------------------------------------+-------------------------+--------+
# | ID                                   | Name                    | Status |
# +--------------------------------------+-------------------------+--------+
# | f5306fd5-73fa-433f-bfe8-20bb3c807893 | CentOS-7.1-x86_64       | active |
# | 8e185e26-e56f-4a69-a37f-ceeaf536fec6 | FreeBSD-10.0-x86_64     | active |
# | c6fa7e35-d728-4ddd-9279-87e84cbabc1f | Ubuntu-14.04-LTS-x86_64 | active |
# | 0d30a10f-3b05-4421-8ed0-43f62e85c961 | Ubuntu-15.04-x86_64     | active |
# | 10015e39-9494-4b6c-99e4-1341ef311188 | Ubuntu-16.04-LTS-x86_64 | active |
# | d359996b-beb7-4aae-b07a-59f8b2b72119 | cirros-0.3.4-x86_64     | active |
# +--------------------------------------+-------------------------+--------+
# flavors:
# m1.tiny 1 CPU 512 Mb memory 1 Gb disk
# m1.small 1 CPU 2Gb memory 20 Gb disk
# m1.medium 2 CPU 4 Gb memory 40 Gb disk
# m1.large 4 SPU 8 Gb memory 80 Gb disk
# m1.xlarge 8 CPU 16 Gb memory 160 Gb disk
# z1.small 6 CPU 50 Gb memory 2 Tb disk
# z1.medium 15 CPU 128 Gb memory 5 Tb disk
# z1.huge 31 CPU 250 Gb memory 10 Tb disk

export imageType="centos"
export imageId="f5306fd5-73fa-433f-bfe8-20bb3c807893"
# export flavor="z1.huge"
export flavor="z1.medium"
# export flavor="m1.medium"
# export flavor="z1.small"
export nodeType="paraNode"

# this startScript is passed to the machine instance to use as
# the machine boots to get work done on the machine to make it ready
# for this parasol node task
export startScript="${nodeType}Setup.sh"

# maintain start up records in ./logs/ directory:
mkdir -p logs

# our system admins like to know who started this image, add userName
# to the image name
userName=`id -u -n`
dateStamp=`date "+%FT%T" | sed -e 's/-//g; s/://g;'`
export hostName="${userName}Node_${nodeId}"
export logFile="logs/start.${hostName}.${flavor}.${dateStamp}.txt"

#########################################################################
dividingLine > ${logFile}

printf "### openstack server create ${hostName} \
   --image "${imageId}" \
   --flavor ${flavor} --key-name ${keyName} \
   --user-data ${startScript}\n" >> ${logFile}
#########################################################################
dividingLine >> ${logFile}

openstack server create ${hostName} \
   --image "${imageId}" \
   --flavor ${flavor} --key-name ${keyName} \
   --user-data ${startScript} >> ${logFile}

#########################################################################
dividingLine >> ${logFile}

# It takes at least 4 minutes to be available
printf "# waiting 280 seconds for machine to start %s\n" "`date '+%F %T'`" 1>&2
sleep 280

printf "### openstack server list\n" >> ${logFile}
openstack server list >> ${logFile}

#########################################################################
dividingLine >> ${logFile}

export instanceId=`openstack server list | grep "${hostName}" | awk '{print $2}'`
# create new floating IP address:
export floatIp=`openstack floating ip create ext-net | grep floating_ip_address | awk '{print $4}'`

# add it to the just started image
printf "### openstack server add floating ip $instanceId $floatIp\n" >> ${logFile}
openstack server add floating ip $instanceId $floatIp 

if [ $? -eq 0 ]; then
  printf "### successfully started ${hostName} as ${instanceId} at ${floatIp}\n"
  printf "### successfully started ${hostName} as ${instanceId} at ${floatIp}\n" >> ${logFile}
else
  printf "### ERROR FAILED started ${hostName}\n"
  printf "### ERROR FAILED started ${hostName}\n" >> ${logFile}
fi

#########################################################################
dividingLine >> ${logFile}

printf "### access the %s server at: ssh %s@%s\n" "${nodeType}" "${imageType}" "${floatIp}"

#########################################################################
dividingLine >> ${logFile}

printf "### openstack server list\n" >> ${logFile}
openstack server list >> ${logFile}

# the machine may have started some time ago, but it isn't actually
# available until it is done with all of its initialization work and
# then finally the sshd daemon is responding, and the
# signal file /tmp/node.machine.ready.signal is present.  Wait for that.

waitSshKeys "${imageType}" "${floatIp}" 2>&1 | tee -a >> ${logFile}

# in the paraNode instances:
export nodeUser="${imageType}@${floatIp}"

printf "### machine completed startup at:\t"
eval "${sshCmd}" "${nodeUser}" 'cat /tmp/node.machine.ready.signal'
printf "### machine completed startup at:\t" >> ${logFile}
eval "${sshCmd}" "${nodeUser}" 'cat /tmp/node.machine.ready.signal' \
   >> ${logFile}

# wake up the NFS filesystem with a directory touch, this eliminates
# intermittant missing nodeReport.sh script
eval "${sshCmd}" "${nodeUser}" 'touch /data/parasol/nodeInfo'

# use the ssh keys generated by the parasol hub machine
# this will allow all machines to ssh to each other without prompts

# this copy is taking place here because the startScript can not perform
# this copy as the 'root' user that it is.  This is a protected file
# at 600 permissions and the 'root' account can not access NFS files that
# are that restricted.  The id_rsa.pub was copied by the start script
# and added to the authorized_hosts

eval "${sshCmd}" "${nodeUser}" 'cp -p /data/parasol/nodeInfo/id_rsa /home/centos/.ssh/id_rsa'

# this 'nodeReport.sh 0' reports this parasol node to the parasol hub
# with 0 reserved CPUs.  All CPUs in this node will be available.

eval "${sshCmd}" "${nodeUser}" '/data/parasol/nodeInfo/nodeReport.sh 0'

#########################################################################
dividingLine >> ${logFile}
#########################################################################
