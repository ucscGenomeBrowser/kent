#!/bin/bash

set -ebEu -o pipefail

export hostname=""
hostname=$(hostname -s)
if [ "${hostname}" != "hubspace" ]; then
    printf "stopping because not on hubspace machine\n"
    exit
fi

export WORKDIR="/data/tusd/data"
export YYYY=`date "+%Y"`
export MM=`date "+%m"`
export DS=`date "+%Y-%m-%dT%H"`

LOGDIR="/home/qateam/hubSpace/${YYYY}/${MM}"
LOG="${LOGDIR}/${DS}.txt"
export LOGDIR LOG

mkdir -p ${LOGDIR}

# tally the total number of uploaded files
printf "# total number of uploaded files\n" >> ${LOG}
find ${WORKDIR} -type f | wc -l >> ${LOG}

# tally the total disk space
printf "# total disk space consumed\n" >> ${LOG}
du -sch ${WORKDIR} >> ${LOG}

# tally the file type counts based on extension only
printf "# file type counts\n" >> ${LOG}
find ${WORKDIR} -type f | tr '/' '\t' | cut -f8 | tr '.' '\t' | awk '{print $NF}' | sort | uniq -c >> ${LOG}

# tally the users
printf "# userNames of uploaders\n" >> ${LOG}
find ${WORKDIR} -mindepth 2 -maxdepth 2 -type d | tr '/' '\t' | cut -f6- >> ${LOG}
