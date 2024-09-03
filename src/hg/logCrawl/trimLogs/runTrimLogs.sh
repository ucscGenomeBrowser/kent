#!/bin/bash

############################
# meant to be run via cronjob
############################

set -beEu -o pipefail

WORKDIR="/hive/users/chmalee/logs/trimmedLogs"
EMAIL="chmalee@ucsc.edu"
GENSUB="/cluster/bin/x86_64/gensub2"

# work dir
today=`date +%F`

# which step of the script are we at
trimStep=1

# which cluster to use, default to ku but can use hgwdev in a pinch
cluster="ku"

# force a re-run on all files, not just the new ones
force=1

function usage()
{
cat << EOF
Usage: `basename $0` [-htc]

Optional Arguments:
-h                  Show this help
-t                  Trim error logs. Smart enough to only run on most recent additions.
-f                  Force a re-run on all files, not just the newest ones
-c                  Use hgwdev instead of ku for cluster run

This script is meant to be run via cronjob to check for new error logs and trim them
down via parasol (the -t option). Checks /hive/data/inside/wwwstats/RR/ for new error_log files
and trims them via the errorLogTrim script. Run that script with no args for more
information.
EOF
}

function combineTrimmed()
{
    # when the jobs are done combine the result files into one:
    fileList=$1
    cd ${WORKDIR}/${today}
    for f in $(cat ${fileList})
    do
        fName=`echo $f | grep -o "hgw.*"`
        fName=`echo ${fName} | sed -e 's/\.gz$//'`
        #echo ${fName}
        cat ${WORKDIR}/result/${fName}.trimmed.gz >> ${WORKDIR}/result/full.gz
    done
}

function getMachName()
{
    # the ## strips the longest match from beginning of string,
    # while the % strips the shortest match from the end, thus
    # fname=/hive/data/inside/wwwstats/RR/2020/hgw1/error_log.20200105.gz will
    # become: hgw1
    fname=$1
    mach=""
    if [[ "${fname}" == *"euroNode"* ]]
    then
        mach="euroNode"
    elif [[ "${fname}" == *"asiaNode"* ]]
    then
        mach="asiaNode"
    else
        mach=${f##*hgw}
        mach="hgw"${mach%/error_log*}
    fi
    # bash does not have return so we echo and capture in the caller
    echo $mach
}


# substitute for gensub2 because asia/euro node have different
# format than RR machine files
function makeJobList() {
    fname=$1
    machName=$(getMachName $fname)
    root1=${fname##*error_log.}
    root1=${root1%.gz}
    echo "../trimLogs.sh ${fname} {check out exists ../result/${machName}/${root1}.trimmed.gz}" >> jobList
}

function runPara()
{
    cd ${WORKDIR}/${today}
    if [ -e jobFileList ]
    then
        for f in $(cat jobFileList); do
            makeJobList $f
        done
        if [[ "${cluster}" == "ku" ]]
        then
            ssh ku "cd ${WORKDIR}/${today}; para create jobList; para push; exit"
        else
            para create -ram=10g jobList
            para push -maxJob=10
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
    set +e
    sort ${WORKDIR}/${oldDir}/jobFileList > jobFileList.prev
    find /hive/data/inside/wwwstats/RR/{2019,202*}/hgw{1,2,3,4,5,6}/error_log.*.gz -print 2>/dev/null | sort > rr.tmp
    find /hive/data/inside/wwwstats/{euroNode,asiaNode}/{2019,202*}/error_log.*.gz -print 2>/dev/null | sort > asiaEuro.tmp
    sort rr.tmp asiaEuro.tmp > allFileList
    rm rr.tmp asiaEuro.tmp

    set -e
    if [[ ${force} -ne 0 ]]
    then
        cp jobFileList.prev jobFileList.tmp
        # the most recently checked logs plus the new ones:
        comm -13 ../${oldDir}/allFileList allFileList >> jobFileList.tmp
        # every time this script is run, we need to force a re-run on the last weeks' logs,
        # as they may have only been partially complete when we last ran the cluster job:
        lastWeek=`date -d "21 days ago" +%s`
        for f in `cat allFileList`
        do
            machName=$(getMachName $f)
            r=${f##*error_log.}
            r=${r%.gz}
            range=`date -d "${r}" +%s`
            if [ ${range} -gt ${lastWeek} ]
            then
                toRemove="../result/${machName}/${r}.trimmed.gz"
                if [ -e ${toRemove} ]; then
                    rm ${toRemove}
                fi
                echo $f >> jobFileList
            fi
            # if it's a brand new file we need to run it!
            if [ ! -e "../result/${machName}/${r}.trimmed.gz" ];
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
            machName=$(getMachName $f)
            r=${f##*error_log.}
            r=${r%.gz}
            toRemove="../result/${machName}/${r}.trimmed.gz"
            if [ -e ${toRemove} ]; then
                rm ${toRemove}
            fi
            set -e
        done
        cp allFileList jobFileList
    fi
}

function doTrimStep()
{
    getFileList
    runPara
}

while getopts "htcf" opt
do
    case $opt in
        h)
            usage
            exit 0
            ;;
        t)
            trimStep=0
            ;;
        c)
            cluster="hgwdev"
            ;;
        f)
            force=0
            ;;
        ?)
            printf "unknown option %s\n" "$opt"
            EXIT_STATUS=1
            ;;
    esac
done

if [[ $# -eq 0 && -n "${trimStep}" ]]
then
    printf "please run with -t\n"
    usage
    trimStep=1
    force=1
fi

if [[ ${trimStep} -eq 0 ]]
then
    doTrimStep
fi

if [[ $? -eq 0 ]]
then
    echo "Done trimming logs"
else
    echo "Potential error during log trimming. Check ${WORKDIR}/${today} for more information."
fi
