#!/bin/bash
#
# startParaNode.sh - start an OpenStack machine instance configured for
#                   parasol node operations
#

usage() {
  printf "usage: ./startParaNode.sh <credentials.txt> <internal IP for paraHub> <id number 0-9>
where <credentials.txt> is a set of parameters required for OpenStack instances
      see example credentials.txt for information for these parameters
the internal IP address would have been shown during the startParaHub.sh
    script, or you can look it up in the AWS EC2 console on that hub instance
the id number is merely used to name this node to keep track of the
    different node instances
e.g: ./startParaNode.sh credentials.txt 172.31.23.44 3\n" 1>&2
  exit 255;
}
# the 1>&2 makes the printf output to stderr

if [ $# -lt 3 ]; then
  usage
fi

export credentials=$1
export hubIpAddr=$2
export nodeId=$3

if [ "X${credentials}Y" = "XY" ]; then
    usage
fi

if [ -f "${credentials}" ]; then
    . "${credentials}"
else
    printf "ERROR: startParaNode.sh: can not find ${credentials}\n" 1>&2
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

printf "# setting up for hub: %s\n" "${hubIpAddr}" 1>&2

# this startScript is passed to the machine instance to use as
# the machine boots to get work done on the machine to make it ready
# for this parasol node task
export startScript="${parasolNodeType}Setup.sh"

# haven't yet figured out how to pass arguments to this script in the
# machine instance.  In the meantime, customize the startup script with
# the arguments we know about here.
sed -e "s/PARAHUB/$hubIpAddr/g;" nodeSetup.sh.template > "${startScript}"

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
    eval "${sshCmd}" "${nodeLogin}" 'cat /tmp/node.machine.ready.signal'
    returnCode=$?
    printf "### returned from 'ssh ${nodeLogin}' returnCode: $returnCode\n" 1>&2
    waitSeconds=`echo $waitSeconds | awk '{print $1+1}'`
  done
  printf "### wait on 'ssh ${nodeLogin} is complete\n" 1>&2
}


# maintain start up records in ./logs/ directory:
mkdir -p logs

# our system admins like to know who started this image, add userName
# to the image name
userName=`id -u -n`
dateStamp=`date "+%FT%T" | sed -e 's/-//g; s/://g;'`
export hostName="${userName}Node_${nodeId}"
export logFile="logs/start.${hostName}.${nodeInstanceType}.${dateStamp}.txt"

#########################################################################
dividingLine > ${logFile}

printf "### openstack server create ${hostName} \
   --nic net-id="${networkId}" \
   --security-group "${secGroupId}" \
   --image "${nodeAmiImageId}" \
   --flavor ${nodeInstanceType} --key-name ${keyPairName} \
   --user-data ${startScript}\n" >> ${logFile}
#########################################################################
dividingLine >> ${logFile}

openstack server create ${hostName} \
   --nic net-id="${networkId}" \
   --security-group "${secGroupId}" \
   --image "${nodeAmiImageId}" \
   --flavor ${nodeInstanceType} --key-name ${keyPairName} \
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

# maintain logs of this startup procedure to check for errors
#########################################################################
dividingLine >> ${logFile}

export instanceId=`openstack server show "${hostName}" | grep -w id | awk '{print $4}'`
export internalIp=`openstack server show "${hostName}" | grep -w addresses | awk '{print $4}' | sed -e 's#${networkName}=##; s#,##;'`

printf "### openstack server internal ip $instanceId $internalIp\n" >> ${logFile}

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

printf "### access the %s server at: ssh %s@%s\n" "${parasolNodeType}" "${nativeUser}" "${floatIp}"

#########################################################################
dividingLine >> ${logFile}

printf "### openstack server list\n" >> ${logFile}
openstack server list >> ${logFile} 2>&1
printf "### openstack server show ${hostName} \n" >> ${logFile}
openstack server show ${hostName} >> ${logFile}

# the machine may have started some time ago, but it isn't actually
# available until it is done with all of its initialization work and
# then finally the sshd daemon is responding, and the
# signal file /tmp/node.machine.ready.signal is present.  Wait for that.

waitSshKeys "${nativeUser}" "${floatIp}" 2>&1 | tee -a >> ${logFile}

# in the paraNode instances:
export nodeLogin="${nativeUser}@${floatIp}"

printf "### machine completed startup at:\n"
eval "${sshCmd}" "${nodeLogin}" 'cat /tmp/node.machine.ready.signal'
printf "### machine completed startup at:\n" >> ${logFile}
eval "${sshCmd}" "${nodeLogin}" 'cat /tmp/node.machine.ready.signal' \
   >> ${logFile}

# wake up the NFS filesystem with a directory touch, this eliminates
# intermittant missing nodeReport.sh script
eval "${sshCmd}" "${nodeLogin}" 'touch /data/parasol/nodeInfo' >> ${logFile}

# use the ssh keys generated by the parasol hub machine
# this will allow all machines to ssh to each other without prompts

# this copy is taking place here because the startScript can not perform
# this copy as the 'root' user that it is.  This is a protected file
# at 600 permissions and the 'root' account can not access NFS files that
# are that restricted.  The id_rsa.pub was copied by the start script
# and added to the authorized_hosts

eval "${sshCmd}" "${nodeLogin}" 'cp -p /data/parasol/nodeInfo/id_rsa \$HOME/.ssh/id_rsa' >> ${logFile}

# this 'nodeReport.sh 0' reports this parasol node to the parasol hub
# with 0 reserved CPUs.  All CPUs in this node will be available.

eval "${sshCmd}" "${nodeLogin}" '/data/parasol/nodeInfo/nodeReport.sh 0 /data/parasol/nodeInfo' >> ${logFile}

#########################################################################
dividingLine >> ${logFile}
#########################################################################
