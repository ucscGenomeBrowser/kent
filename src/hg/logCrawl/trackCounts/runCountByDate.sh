#!/bin/bash

############################
# meant to be run via cronjob
############################

set -beEu -o pipefail

WORKDIR="/hive/users/chmalee/logs/byDate"
EMAIL="chmalee@ucsc.edu"
GENSUB="/cluster/bin/x86_64/gensub2"

# work dir
today=`date +%F`

# force a re-run on all files, not just the new ones
force=1

# cluster to use, default to ku
cluster=ku

function usage()
{
cat << EOF
Usage: `basename $0` [-hfc]

Optional Arguments:
-h                  Show this help
-f                  Force a re-run on all files, not just the newest ones
-c                  Use hgwdev as a cluster instead of ku

This script is meant to be run via cronjob to check for new trimmed error logs and tally
up track and database usage
EOF
}

function runPara()
{
    cd ${WORKDIR}/${today}
    if [ -e jobFileList ]
    then
        ${GENSUB} jobFileList single ../template jobList
        if [[ ${cluster} == "ku" ]]
        then
            ssh ku "cd ${WORKDIR}/${today}; para create -ram=20g jobList; para push; exit"
        else
            /cluster/bin/x86_64/para create -ram=20g jobList
            /cluster/bin/x86_64/para push -maxJob=10
        fi
    fi
}

function getFileList()
{
    cd ${WORKDIR}
    # find the most recent dir, example %T+ output
    #2019-10-21+09:41:17.5243280000  ./2019-10-21
    oldDir=`find . -maxdepth 1 -type d -name "20*" -printf "%T+\t%p\n" | sort -r | head -1 | cut -f2`
    mkdir -p ${WORKDIR}/${today}
    cd ${WORKDIR}/${today}
    if [ -e ${WORKDIR}/${oldDir}/jobFileList ]
    then
        sort ${WORKDIR}/${oldDir}/jobFileList > jobFileList.prev
    else
        touch jobFileList.prev
    fi
    set +e
    find /hive/users/chmalee/logs/trimmedLogs/result/{asiaNode,euroNode,hgw{1,2,3,4,5,6}}/*trimmed.gz -print 2>/dev/null | sort > allFileList

    set -e
    if [[ ${force} -ne 0 ]]
    then
        cp jobFileList.prev jobFileList.tmp
        # the most recently checked logs plus the new ones:
        comm -13 ../${oldDir}/allFileList allFileList >> jobFileList.tmp
        # every time this script is run, we need to force a re-run on the last weeks' logs,
        # as they may have only been partially complete when we last ran the cluster job:
        lastWeek=`date -d "50 days ago" +%s`
        for f in `cat allFileList`
        do
            r=${f%.trimmed.gz}
            r=${r##*/}
            machName=${f##*result/}
            machName=${machName%/*}
            range=`date -d "${r}" +%s`
            printf "checking if range %s > lastWeek %s for %s %s %s\n" $range $lastWeek $r $machName $f >> debug.log
            if [ ${range} -gt ${lastWeek} ]
            then
                printf "is greater\n" >> debug.log
                toRemoveDb="../result/${machName}/${r}.trimmed.dbUsage.gz"
                toRemoveTrack="../result/${machName}/${r}.trimmed.trackUsage.gz"
                printf "should be removing %s\n" $toRemoveDb >> debug.log
                printf "should be removing %s\n" $toRemoveTrack >> debug.log
                if [ -e ${toRemoveTrack} ]; then
                    printf "removing %s\n" $toRemoveTrack >> debug.log
                    rm ${toRemoveTrack}
                fi
                if [ -e ${toRemoveDb} ]; then
                    printf "removing %s\n" $toRemoveDb >> debug.log
                    rm ${toRemoveDb}
                fi
                #echo $f >> jobFileList
            fi
            # if it's a brand new file we need to run it!
            if [ ! -e "../result/${machName}/${r}.trimmed.*.gz" ];
            then
                echo $f >> jobFileList
            fi
        done
        if [ -e jobFileList ]
        then
            rm jobFileList.tmp
        else
            mv jobFileList.tmp jobFileList
        fi
    else
        for f in `cat allFileList`
        do
            set +e
            r=${f%.trimmed.gz}
            r=${r##*/}
            machName=${f##*result/}
            machName=${machName%/*}
            toRemoveDb="../result/${machName}/${r}.trimmed.dbUsage.gz"
            toRemoveTrack="../result/${machName}/${r}.trimmed.trackUsage.gz"
            if [ -e ${toRemoveTrack} ]; then
                printf "removing %s\n" $toRemoveDb >> debug.log
                rm ${toRemoveTrack}
            fi
            if [ -e ${toRemoveDb} ]; then
                printf "removing %s\n" $toRemoveDb >> debug.log
                rm ${toRemoveDb}
            fi
            set -e
        done
        cp allFileList jobFileList
    fi
}

while getopts "hfc" opt
do
    case $opt in
        h)
            usage
            exit 0
            ;;
        f)
            force=0
            ;;
        c)
            cluster="hgwdev"
            ;;
        ?)
            printf "unknown option %s\n" "$opt"
            EXIT_STATUS=1
            ;;
    esac
done

getFileList
runPara

echo "Done tallying usage"
