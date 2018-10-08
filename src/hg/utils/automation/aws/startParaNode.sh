#!/bin/bash
#
# startParaNode.sh - start an Amazon/AWS machine instance configured for
#                   parasol node operations
#

usage() {
  printf "usage: ./startParaNode.sh <credentials.txt> <internal IP for paraHub> <id number 0-9>
where <credentials.txt> is a set of parameters required for AWS instances
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

# global default variables not subject to customization
export AWS_DEFAULT_OUTPUR=text

# credentials file includes these settings:
# export AWS_CONFIG_FILE
# export AWS_DEFAULT_REGION
# export nativeUser
# export hubAmiImageId
# export nodeAmiImageId
# export hubInstanceType
# export nodeInstanceType
# export keyName
# export secGroupId
# export subnetId

# would like to use this to fail on any error, but there seem to be
#  too many failed commands that are testing things to work around that
# set -beEu -o pipefail

# to use a command like this in a variable, need to use 'eval'
export sshCmd="ssh -i ~/.ssh/aws_rsa -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes'"

printf "# setting up for hub: %s\n" "${hubIpAddr}" 1>&2

# this startScript is passed to the machine instance to use as
# the machine boots to get work done on the machine to make it ready
# for this parasol node task
export startScript="nodeSetup.sh"

# haven't yet figured out how to pass arguments to this script in the
# machine instance.  In the meantime, customize the startup script with
# the arguments we know about here.
sed -e "s/PARAHUB/$hubIpAddr/g;" "${startScript}.template" > "${startScript}"

############################################################################
function dividingLine {
  dateTag=`date "+%s %F %T"`
  printf "############  %s ######################\n" "$dateTag"
}

############################################################################
# waiting here for the sshd to respond in the newly started image
# going to wait for the presence of the ready.signal file in the instance
# itself created by the start up script given to the instance
function waitSshKeys {
  export userName=$1
  export ip=$2
  export nodeUser="${userName}@${ip}"
  export returnCode=1
  export waitSeconds=5
  while [ "$returnCode" -ne 0 ]
  do
    sleep $waitSeconds
    printf "### waiting ${waitSeconds}s on 'ssh -i ~/.ssh/aws_rsa ${nodeUser}'\n" 1>&2
    eval "${sshCmd}" "${nodeUser}" 'cat /tmp/node.machine.ready.signal'
    returnCode=$?
    printf "### returned from 'ssh -i ~/.ssh/aws_rsa ${nodeUser}' returnCode: $returnCode\n" 1>&2
    waitSeconds=`echo $waitSeconds | awk '{print $1+1}'`
  done
  printf "### wait on 'ssh -i ~/.ssh/aws_rsa ${nodeUser} is complete\n" 1>&2
}

############################################################################
# temporary file with the results from the instance start command
# to be used to obtain the instanceId for the started instance
export instResponseFile="instanceResult.$$"

# maintain start up records in ./logs/ directory:
mkdir -p logs

# our system admins like to know who started this image, add userName
# to the image name
userName=`id -u -n`
dateStamp=`date "+%FT%T" | sed -e 's/-//g; s/://g;'`
export hostName="${userName}Node_${nodeId}"
export logFile="logs/start.${hostName}.${nodeAmiImageId}.${dateStamp}.txt"

#########################################################################
dividingLine > ${logFile}

printf "## starting instance at %s\n" "`date '+%s %F %T'`" 1>&2
printf "## starting instance at %s" "`date '+%s %F %T'`" >> "${logFile}" 2>&1

printf "### aws ec2 run-instances --image-id \"${nodeAmiImageId}\" --count 1 \
  --instance-type \"${nodeInstanceType}\" --key-name \"${keyName}\" \
    --security-group-ids \"${secGroupId}\" --subnet-id \"${subnetId}\" \
      --user-data file://${startScript} --tag-specifications \
	\"ResourceType=instance,Tags=[{Key=Name,Value=$hostName},{Key=PARAHUB,Value=$hubIpAddr}]\"\n" \
           >> "${logFile}"

#########################################################################
dividingLine >> ${logFile}

# this starts the machine image

aws ec2 run-instances --image-id "${nodeAmiImageId}" --count 1 \
  --instance-type "${nodeInstanceType}" --key-name "${keyName}" \
    --security-group-ids "${secGroupId}" --subnet-id "${subnetId}" \
      --user-data file://${startScript} --tag-specifications \
	"ResourceType=instance,Tags=[{Key=Name,Value=$hostName},{Key=PARAHUB,Value=$hubIpAddr}]" \
           > "${instResponseFile}"

#########################################################################
dividingLine >> ${logFile}

printf "#### run-instances response:\n" >> "${logFile}"
cat "${instResponseFile}" >> "${logFile}" 2>&1

#########################################################################
dividingLine >> ${logFile}

export instanceId=`grep InstanceId "${instResponseFile}" | head -1 | awk '{print $NF}' | sed -e 's/"//g; s/,//;'`
printf "#### instance status instanceId: ${instanceId}\n" >> "${logFile}"

aws ec2 describe-instance-status --instance-id "${instanceId}" >> "${logFile}"
# now sure how long it takes before the external IP is available ?

# it can take over 15 minutes for all the software to install
printf "# waiting 300 seconds for machine to start %s\n" "`date '+%F %T'`" 1>&2
sleep 300

#########################################################################
dividingLine >> ${logFile}

aws ec2 describe-instances --instance-id "${instanceId}" >> "${logFile}"

export privateIp=`aws ec2 describe-instances --instance-id "${instanceId}" --query Reservations[].Instances[].PrivateIpAddress | grep '"' | sed -e 's/"//g; s/ \+//;'`
export publicIp=`aws ec2 describe-instances --instance-id "${instanceId}" --query Reservations[].Instances[].PublicIpAddress | grep '"' | sed -e 's/"//g; s/ \+//;'`

printf "# login to the instance: ssh -i ~/.ssh/aws_rsa ${nativeUser}@${publicIp}\n"

#########################################################################
dividingLine >> ${logFile}

# the machine may have started some time ago, but it isn't actually
# available until it is done with all of its initialization work and
# then finally the sshd daemon is responding, and the
# signal file /tmp/hub.machine.ready.signal is present.  Wait for that.

waitSshKeys "${nativeUser}" "${publicIp}" 2>&1 | tee -a >> ${logFile}

# in the paraNode instances:
export nodeUser="${nativeUser}@${publicIp}"

printf "### machine completed startup at:\n"
eval "${sshCmd}" "${nodeUser}" 'cat /tmp/node.machine.ready.signal'
printf "### machine completed startup at:\n" >> ${logFile}
eval "${sshCmd}" "${nodeUser}" 'cat /tmp/node.machine.ready.signal' \
   >> ${logFile}

# wake up the NFS filesystem with a directory touch, this eliminates
# intermittant missing nodeReport.sh script
eval "${sshCmd}" "${nodeUser}" 'touch /data/parasol/nodeInfo' >> ${logFile}

# use the ssh keys generated by the parasol hub machine
# this will allow all machines to ssh to each other without prompts

# this copy is taking place here because the startScript can not perform
# this copy as the 'root' user that it is.  This is a protected file
# at 600 permissions and the 'root' account can not access NFS files that
# are that restricted.  The id_rsa.pub was copied by the start script
# and added to the authorized_hosts

eval "${sshCmd}" "${nodeUser}" 'cp -p /data/parasol/nodeInfo/id_rsa \$HOME/.ssh/id_rsa' >> ${logFile}

# this 'nodeReport.sh 0' reports this parasol node to the parasol hub
# with 0 reserved CPUs.  All CPUs in this node will be available.

eval "${sshCmd}" "${nodeUser}" '/data/parasol/nodeInfo/nodeReport.sh 0 /data/parasol/nodeInfo' >> ${logFile}

#########################################################################
dividingLine >> ${logFile}
#########################################################################
# temporary file no longer needed
rm -f "${instResponseFile}"
