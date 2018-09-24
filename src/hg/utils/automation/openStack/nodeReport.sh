#!/bin/bash
# construct .ms file from this node for parasol hub controller
reserveCPUs=$1
myIp=`/usr/sbin/ip addr show | egrep "inet.*eth0|inet.*em1" | awk '{print $2}' | sed -e 's#/.*##;'`
memSizeMb=`grep -w MemTotal /proc/meminfo  | awk '{print 1024*(1+int($2/(1024*1024)))}'`
cpuCount=`grep processor /proc/cpuinfo | wc -l`
useCPUs=$(expr $cpuCount - $reserveCPUs)
if [ "${useCPUs}" -lt 1 ]; then
useCPUs=1
fi
memTotal=`echo $memSizeMb $cpuCount | awk '{printf "%d", $1, $2}'`
devShm=`df -k /dev/shm | grep dev/shm | awk '{printf "%d\n", 512*(1+int($2/(1024*1024)))}'`
printf "%s\t%d\t%d\t/dev/shm\t/dev/shm\t%s\tbsw\n" "${myIp}" "${useCPUs}" "${memTotal}" "${devShm}" > /data/parasol/nodeInfo/${myIp}.ms
