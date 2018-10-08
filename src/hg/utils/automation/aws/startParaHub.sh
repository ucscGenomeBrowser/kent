#!/bin/bash
#
# startParaHub.sh - start an Amazon/AWS machine instance configured for
#                   parasol hub operations
#
usage() {
  printf "usage: ./startParaHub.sh <credentials.txt> <hubId>
where <credentials.txt> is a set of parameters required for AWS instances
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

# this private key file should be some kind of parameter TBD
if [ ! -s ~/.ssh/aws_rsa ]; then
  printf "ERROR: expecting to find your private ssh key in ~/.ssh/aws_rsa\n" 1>&2
  exit 255
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

# global default variables not subject to customization
export AWS_DEFAULT_OUTPUR=text
# tried to use this sshCmd everwhere, but can't get it to work
# with the required eval.  It works only with simple single commands.
# Could not figure out compound commands with complicated quoting.
export sshCmd="ssh -i ~/.ssh/aws_rsa  -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes'"

# credentials file includes these settings:
# export AWS_CONFIG_FILE
# export AWS_DEFAULT_REGION
# export nativeUser
# export hubAmiImageId
# export nodeAmiImageId
# export hubInstanceType
# export nodeInstanceType
# export dataVolumeId
# export keyName
# export secGroupId
# export subnetId

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
    printf "### waiting ${waitSeconds}s on 'ssh ${nodeUser}'\n" 1>&2
    eval "${sshCmd}" "${nodeUser}" 'cat /tmp/hub.machine.ready.signal'
    returnCode=$?
    printf "### returned from 'ssh ${nodeUser}' returnCode: $returnCode\n" 1>&2
    waitSeconds=`echo $waitSeconds | awk '{print $1+1}'`
  done
  printf "### wait on 'ssh ${nodeUser} is complete\n" 1>&2
}

############################################################################
# temporary file with the results from the instance start command
# to be used to obtain the instanceId for the started instance
export instResponseFile="instanceResult.$$"

# this startScript is passed to the machine instance to use as
# the machine boots to get work done on the machine to make it ready
# for this parasol hub task

export startScript="hubStart.txt"

# maintain start up records in ./logs/ directory:
mkdir -p logs

# our system admins like to know who started this image, add userName
# to the image name
userName=`id -u -n`
dateTag=`date "+%s %F %T"`
dateStamp=`date "+%FT%T" | sed -e 's/-//g; s/://g;'`
export hostName="${userName}Hub_${hubId}"
export logFile="logs/start.${hostName}.${hubAmiImageId}.${dateStamp}.txt"

# maintain logs of this startup procedure to check for errors

#########################################################################
dividingLine >> ${logFile}

printf "## starting instance at %s\n" "`date '+%s %F %T'`" 1>&2
printf "## starting instance at %s" "`date '+%s %F %T'`" >> "${logFile}" 2>&1

printf "### aws ec2 run-instances --image-id \"${hubAmiImageId}\" --count 1 \
  --instance-type \"${hubInstanceType}\" --key-name \"${keyName}\" \
    --security-group-ids \"${secGroupId}\" --subnet-id \"${subnetId}\" \
      --user-data file://${startScript} --tag-specifications \
	\"ResourceType=instance,Tags=[{Key=Name,Value=$hostName},{Key=volumeId,Value=$dataVolumeId}]\"\n" \
           >> "${logFile}"

# The tags can be viewed in the instance with:
# aws ec2 describe-tags --filters "Name=resource-id,Values=(this instance id)"
# to get the instance-id
# curl http://169.254.169.254/latest/meta-data/instance-id

#########################################################################
dividingLine >> ${logFile}

# this starts the machine image
aws ec2 run-instances --image-id "${hubAmiImageId}" --count 1 \
  --instance-type "${hubInstanceType}" --key-name "${keyName}" \
    --security-group-ids "${secGroupId}" --subnet-id "${subnetId}" \
      --user-data file://${startScript} --tag-specifications \
	"ResourceType=instance,Tags=[{Key=Name,Value=$hostName},{Key=volumeId,Value=$dataVolumeId}]" \
           > "${instResponseFile}" 2>&1

#########################################################################
dividingLine >> ${logFile}

printf "#### run-instances response:\n" >> "${logFile}"
cat "${instResponseFile}" >> "${logFile}" 2>&1

#########################################################################
dividingLine >> ${logFile}

export instanceId=`grep InstanceId "${instResponseFile}" | head -1 | awk '{print $NF}' | sed -e 's/"//g; s/,//;'`
printf "#### instance status instanceId: ${instanceId}\n" >> "${logFile}"

aws ec2 describe-instance-status --instance-id "${instanceId}" >> "${logFile}" 2>&1
# now sure how long it takes before the external IP is available ?

# it can take over 15 minutes for all the software to install
sleep 300

#########################################################################
dividingLine >> ${logFile}

aws ec2 describe-instances --instance-id "${instanceId}" >> "${logFile}" 2>&1

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

# generate ssh keys to share with all parasol nodes
export nodeUser="${nativeUser}@${publicIp}"

printf "### machine completed startup:\n"
eval "${sshCmd}" "${nodeUser}" 'cat /tmp/hub.machine.ready.signal'
printf "### machine completed startup:\n" >> ${logFile}
eval "${sshCmd}" "${nodeUser}" 'cat /tmp/hub.machine.ready.signal' \
   >> ${logFile} 2>&1

# these ssh commands didn't work with the eval:
#       eval "${sshCmd}" "${nodeUser}" 'command'
# much confusion with interaction between the eval and the
# quoting in the commands

# these ssh keys are going to be used by all the machines in this
# parasol system to allow them to all ssh to each other without prompts
# and to themselves as localhost without prompts

ssh -i ~/.ssh/aws_rsa -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeUser}" 'printf "\n" | ssh-keygen -N "" -t rsa -q' >> ${logFile} 2>&1

# now can get into it via ssh, save the ssh keys in the filesystem that
# will be mounted by all the parasol nodes where they can pick them up
# during their initialization

ssh -i ~/.ssh/aws_rsa -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeUser}" 'cp -p .ssh/id_rsa* /data/parasol/nodeInfo/' >> ${logFile} 2>&1

# enable the machine to be able to ssh localhost without prompts:

ssh -i ~/.ssh/aws_rsa -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeUser}" 'cat .ssh/id_rsa.pub >> .ssh/authorized_keys' >> ${logFile} 2>&1

# insert the AWS keys into the .bashrc file to allow aws commands
# to function on the machine
export accessKey=`grep aws_access_key_id "${AWS_CONFIG_FILE}" | awk '{print $NF}'`
export secretKey=`grep aws_secret_access_key "${AWS_CONFIG_FILE}" | awk '{print $NF}'`
ssh -i ~/.ssh/aws_rsa -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' \
    "${nodeUser}" "sed -i -e s/AWS_AccessKey/${accessKey}/ .bashrc" >> ${logFile} 2>&1
ssh -i ~/.ssh/aws_rsa -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' \
    "${nodeUser}" "sed -i -e s/AWS_SecretKey/${secretKey}/ .bashrc" >> ${logFile} 2>&1

ssh -i ~/.ssh/aws_rsa -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' \
    "${nodeUser}" "./bin/attachVolume.sh ${dataVolumeId}" >> ${logFile} 2>&1


# this 'nodeReport.sh 2' will report this parasol hub machine to itself as
# a node with 2 reserved CPUs.

ssh -i ~/.ssh/aws_rsa -x -o 'StrictHostKeyChecking = no' -o 'BatchMode = yes' "${nodeUser}" '/data/parasol/nodeInfo/nodeReport.sh 2 /data/parasol/nodeInfo' >> ${logFile} 2>&1

#########################################################################
dividingLine >> ${logFile}
#########################################################################
# temporary file no longer needed
rm -f "${instResponseFile}"
