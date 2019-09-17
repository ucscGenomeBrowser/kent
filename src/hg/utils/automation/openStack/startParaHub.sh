#!/bin/bash
#
# startParaHub.sh - start an OpenStack machine instance configured for
#                   parasol hub operations
#
# would like to use this to fail on any error, but there seem to be
#  too many failed commands that are testing things to work around that
# set -beEu -o pipefail
#
usage() {
  printf "usage: ./startParaHub.sh <credentials.txt> <hubId>
where <credentials.txt> is a set of parameters required for OpenStack instances
      see example credentials.txt for information for these parameters
where <hubId> is something simple like: 0 1 2 ... to name the hub
you can run more than one parasol hub if you like, but that is
not the normal situation.  More for experiments than anything
else.  Only one parasol hub is needed.\n" 1>&2
  exit 255
# the 1>&2 makes the printf output to stderr
}

if [ $# -lt 2 ]; then
  usage
fi

export credentials=$1
export hubId=$2

if [ "X${credentials}Y" = "XY" ]; then
    usage
fi

if [ -f "${credentials}" ]; then
    . "${credentials}"
else
    printf "ERROR: startParaHub.sh: can not find ${credentials}\n" 1>&2
    usage
fi

# verify SSH private key specified in credentials actually exists
if [ ! -s "${sshPrivateKey}" ]; then
  printf "ERROR: given credentials file: '${credentials}'\n" 1>&2
  printf "\tdoes not correctly specify an SSH private key file\n" 1>&2
  exit 255
fi

# tried to use this sshCmd everwhere, but can't get it to work
# with the required eval.  It works only with simple single commands.
# Could not figure out compound commands with complicated quoting.
export sshCmd="ssh -i $sshPrivateKey -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes'"

function dividingLine {
  dateTag=`date "+%s %F %T"`
  printf "############  %s ######################\n" "$dateTag"
}

# waiting here for the sshd to respond in the newly started image
function waitSshKeys {
  export userName=$1
  export ip=$2
  export nodeLogin="${userName}@${ip}"
  export returnCode=1
  export waitSeconds=5
  while [ "$returnCode" -ne 0 ]
  do
    sleep $waitSeconds
    printf "### waiting ${waitSeconds}s on 'ssh ${nodeLogin}'\n" 1>&2
    eval "${sshCmd}" "${nodeLogin}" 'cat /tmp/hub.machine.ready.signal'
    returnCode=$?
    printf "### returned from 'ssh ${nodeLogin}' returnCode: $returnCode\n" 1>&2
    waitSeconds=`echo $waitSeconds | awk '{print $1+1}'`
  done
  printf "### wait on 'ssh ${nodeLogin} is complete\n" 1>&2
}

# available machine sizes from the command: openstack flavor list
# +-----------+--------+------+-----------+-------+-----------+
# | Name      |    RAM | Disk | Ephemeral | VCPUs | Is Public |
# +-----------+--------+------+-----------+-------+-----------+
# | m1.nano   |     64 |    1 |         0 |     1 | True      |
# | m1.large  |  65536 |   40 |      2048 |    12 | True      |
# | m1.medium |  32768 |   20 |      1024 |     8 | True      |
# | m1.huge   | 120000 |   40 |      2800 |    15 | True      |
# | m1.small  |   5120 |    5 |       250 |     4 | True      |
# | m1.mini   |   1024 |    5 |       100 |     2 | True      |
# +-----------+--------+------+-----------+-------+-----------+

# available images from the command: openstack image list
# +--------------------------------------+-------------------------+--------+
# | ID                                   | Name                    | Status |
# +--------------------------------------+-------------------------+--------+
# | c9eff555-3fc4-466a-a93a-df50fcf8f2ab | centos-7.5-x86_64       | active |
# | 6ce468c2-b2ad-4aa9-ab27-31451461a802 | cirros                  | active |
# | 0407c640-0c9a-4532-b9d1-c9f8c1ca04f7 | ubuntu-16.04-LTS-x86_64 | active |
# | 41f47983-814b-4994-af83-db10a14f01ec | ubuntu-18.04-LTS-x86_64 | active |
# +--------------------------------------+-------------------------+--------+

# variables set in credentials file:
# export hubAmiImageId=".... some identifier ...."
# export nativeUser="centos"
# export hubAmiImageId="... some image identifier ..."
# export nodeAmiImageId="... some image identifier ..."
# export hubInstanceType="m1.large"	# 12 CPUs 64 Gb
# export nodeInstanceType="m1.huge"	# 15 CPUs 120 Gb
# export networkId="... some network identifier ..."
# export networkName="your network name"
# export parasolHubType="paraHub"
# export parasolNodeType="paraNode"

# this startScript is passed to the machine instance to use as
# the machine boots to get work done on the machine to make it ready
# for this parasol hub task

export startScript="${parasolHubType}Setup.sh"

# maintain start up records in ./logs/ directory:
mkdir -p logs

# our system admins like to know who started this image, add userName
# to the image name
userName=`id -u -n`
dateTag=`date "+%s %F %T"`
dateStamp=`date "+%FT%T" | sed -e 's/-//g; s/://g;'`
export hostName="${userName}Hub_${hubId}"
export logFile="logs/start.${hostName}.${hubInstanceType}.${dateStamp}.txt"

# maintain logs of this startup procedure to check for errors
#########################################################################
dividingLine > ${logFile}

printf "### openstack server create ${hostName} \
   --nic net-id="${networkId}" \
   --security-group "${secGroupId}" \
   --image "${hubAmiImageId}" \
   --flavor ${hubInstanceType} --key-name ${keyPairName} \
   --user-data ${startScript}\n" >> ${logFile}

#########################################################################
dividingLine >> ${logFile}

# this starts the machine image

openstack server create ${hostName} \
   --nic net-id="${networkId}" \
   --security-group "${secGroupId}" \
   --image "${hubAmiImageId}" \
   --flavor ${hubInstanceType} --key-name ${keyPairName} \
   --user-data ${startScript} >> ${logFile}

#########################################################################
dividingLine >> ${logFile}

# It takes at least 4 minutes to be available
printf "# recording in logFile: %s\n" "${logFile}" 1>&2
printf "# waiting 280 seconds for machine to start %s\n" "`date '+%F %T'`" 1>&2
sleep 280

printf "### openstack server list\n" >> ${logFile}
openstack server list >> ${logFile} 2>&1
printf "### openstack server show ${hostName} \n" >> ${logFile}
openstack server show ${hostName} >> ${logFile}

#########################################################################
dividingLine >> ${logFile}

export instanceId=`openstack server show "${hostName}" | grep -w id | awk '{print $4}'`
export internalIp=`openstack server show "${hostName}" | grep -w addresses | awk '{print $4}' | sed -e 's#${networkName}=##; s#,##;'`

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

printf "### access the %s server at: ssh %s@%s\n" "${parasolHubType}" "${nativeUser}" "${floatIp}"

#########################################################################
dividingLine >> ${logFile}

printf "### openstack server list\n" >> ${logFile}
openstack server list >> ${logFile} 2>&1
printf "### openstack server show ${hostName} \n" >> ${logFile}
openstack server show ${hostName} >> ${logFile}

# the machine may have started some time ago, but it isn't actually
# available until it is done with all of its initialization work and
# then finally the sshd daemon is responding, and the
# signal file /tmp/hub.machine.ready.signal is present.  Wait for that.

waitSshKeys "${nativeUser}" "${floatIp}" 2>&1 | tee -a >> ${logFile}

# generate ssh keys to share with all parasol nodes
export nodeLogin="${nativeUser}@${floatIp}"

printf "### machine completed startup at:\t"
eval "${sshCmd}" "${nodeLogin}" 'cat /tmp/hub.machine.ready.signal'
printf "### machine completed startup at:\t" >> ${logFile}
eval "${sshCmd}" "${nodeLogin}" 'cat /tmp/hub.machine.ready.signal' \
   >> ${logFile}

# these ssh commands didn't work with the eval:
#       eval "${sshCmd}" "${nodeLogin}" 'command'
# much confusion with interaction between the eval and the
# quoting in the commands

# these ssh keys are going to be used by all the machines in this
# parasol system to allow them to all ssh to each other without prompts
# and to themselves as localhost without prompts

ssh -i "${sshPrivateKey}" -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeLogin}" 'printf "\n" | ssh-keygen -N "" -t rsa -q' 2>&1 >> ${logFile}

# now can get into it via ssh, save the ssh keys in the filesystem that
# will be mounted by all the parasol nodes where they can pick them up
# during their initialization

ssh -i "${sshPrivateKey}" -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeLogin}" 'cp -p .ssh/id_rsa* /data/parasol/nodeInfo/' 2>&1 >> ${logFile}

# enable the machine to be able to ssh localhost without prompts:

ssh -i "${sshPrivateKey}" -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeLogin}" 'cat .ssh/id_rsa.pub >> .ssh/authorized_keys' 2>&1 >> ${logFile}

# the /genomes mount does not appear to take place during the startup of the
# machine, get this filesystem mounted at this time.  'user' mounting of
# this filesystem has been enabled in the /etc/fstab entry:
eval "${sshCmd}" "${nodeLogin}" 'mount /genomes'

# this 'nodeReport.sh 2' will report this parasol hub machine to itself as
# a node with 2 reserved CPUs.

ssh -i "${sshPrivateKey}" -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeLogin}" '/data/parasol/nodeInfo/nodeReport.sh 2 /data/parasol/nodeInfo' 2>&1 >> ${logFile}

#########################################################################
dividingLine >> ${logFile}
#########################################################################
