#!/bin/bash

# would like to use this to fail on any error, but there seem to be
#  too many failed commands that are testing things to work around that
# set -beEu -o pipefail

# tried to use this sshCmd everwhere, but can't get it to work
# with the required eval.  It works only with simple single commands.
# Could not figure out compound commands with complicated quoting.
export sshCmd="ssh -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes'"

# the 1>&2 makes the printf output to stderr

if [ $# -ne 2 ]; then
  printf "usage: ./startParaHub.sh <hubId> <ssh-key-name>\n" 1>&2
  printf "where <hubId> is something simple like: 0 1 2 ...\n" 1>&2
  printf "and <ssh-key-name> is a key pair name you created\n" 1>&2
  printf "you can run more than one parasol hub if you like, but that is\n" 1>&2
  printf "not the normal situation.  More for experiments than anything\n" 1>&2
  printf "else.  Only one parasol hub is needed.\n" 1>&2
  exit 255
fi

export hubId=$1
export keyName=$2

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
    eval "${sshCmd}" "${nodeUser}" 'cat /tmp/hub.machine.ready.signal'
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
# export flavor="m1.xlarge"
export nodeType="paraHub"

# this startScript is passed to the machine instance to use as
# the machine boots to get work done on the machine to make it ready
# for this parasol hub task

export startScript="${nodeType}Setup.sh"

# maintain start up records in ./logs/ directory:
mkdir -p logs

# our system admins like to know who started this image, add userName
# to the image name
userName=`id -u -n`
dateTag=`date "+%s %F %T"`
dateStamp=`date "+%FT%T" | sed -e 's/-//g; s/://g;'`
export hostName="${userName}Hub_${hubId}"
export logFile="logs/start.${hostName}.${flavor}.${dateStamp}.txt"

# maintain logs of this startup procedure to check for errors

#########################################################################
dividingLine > ${logFile}

printf "### openstack server create ${hostName} \
   --image "${imageId}" \
   --flavor ${flavor} --key-name ${keyName} \
   --user-data ${startScript}\n" >> ${logFile}

#########################################################################
dividingLine >> ${logFile}

# this starts the machine image

openstack server create ${hostName} \
   --image "${imageId}" \
   --flavor ${flavor} --key-name ${keyName} \
   --user-data ${startScript} >> ${logFile}

#########################################################################
dividingLine >> ${logFile}

# It takes at least 4 minutes to be available
printf "# recording in logFile: %s\n" "${logFile}" 1>&2
printf "# waiting 280 seconds for machine to start\n" 1>&2
sleep 280

printf "### openstack server list\n" >> ${logFile}
openstack server list >> ${logFile}

#########################################################################
dividingLine >> ${logFile}

export instanceId=`openstack server list | grep "${hostName}" | awk '{print $2}'`
export internalIp=`openstack server list | grep "${hostName}" | awk '{print $8}' | sed -e 's/genomebrowser-net=//;'`
printf "### openstack server internal ip $instanceId $internalIp\n" >> ${logFile}

# create floatingIp to access instance from outside the internal network:

export floatIp=`openstack floating ip create ext-net | grep floating_ip_address | awk '{print $4}'`
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
# signal file /tmp/hub.machine.ready.signal is present.  Wait for that.

waitSshKeys "${imageType}" "${floatIp}" 2>&1 | tee -a >> ${logFile}

# generate ssh keys to share with all parasol nodes
export nodeUser="${imageType}@${floatIp}"

printf "### machine completed startup at:\t"
eval "${sshCmd}" "${nodeUser}" 'cat /tmp/hub.machine.ready.signal'
printf "### machine completed startup at:\t" >> ${logFile}
eval "${sshCmd}" "${nodeUser}" 'cat /tmp/hub.machine.ready.signal' \
   >> ${logFile}

# these ssh commands didn't work with the eval:
#       eval "${sshCmd}" "${nodeUser}" 'command'
# much confusion with interaction between the eval and the
# quoting in the commands

# these ssh keys are going to be used by all the machines in this
# parasol system to allow them to all ssh to each other without prompts
# and to themselves as localhost without prompts

ssh -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeUser}" 'printf "\n" | ssh-keygen -N "" -t rsa -q' 2>&1 >> ${logFile}

# now can get into it via ssh, save the ssh keys in the filesystem that
# will be mounted by all the parasol nodes where they can pick them up
# during their initialization

ssh -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeUser}" 'cp -p .ssh/id_rsa* /data/parasol/nodeInfo/' 2>&1 >> ${logFile}

# enable the machine to be able to ssh localhost without prompts:

ssh -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeUser}" 'cat .ssh/id_rsa.pub >> .ssh/authorized_keys' 2>&1 >> ${logFile}

# this 'nodeReport.sh 2' will report this parasol hub machine to itself as
# a node with 2 reserved CPUs.

ssh -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeUser}" '/data/parasol/nodeInfo/nodeReport.sh 2' 2>&1 >> ${logFile}

#########################################################################
dividingLine >> ${logFile}
#########################################################################
