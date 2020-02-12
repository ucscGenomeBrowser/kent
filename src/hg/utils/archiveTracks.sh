#!/bin/bash

# this script backs up track data (both bigBeds and mysql tables)
# to a location of your choosing

set -beEu -o pipefail

# globals
archiveDir="/hive/data/inside/archive"
tables=""
files=""
versionName=""
trackSetName=""
dbList=""
verbose="FALSE"
checkOnly="FALSE"
EXIT_STATUS=0

usage() {
cat << EOF
Usage: `basename $0` [-hcbtfv] archiveRoot database(s) trackArchiveName

Required Positional arguments:
archiveRoot        The root location of the backup directory (/hive/data/inside/archive/).
database(s)        A single database, a double quoted list of databases, or a file of db
                   names to back up
trackSetName       The name of this track archive set. A directory will be created
                   in the archiveRoot location with the files or tables from the -t
                   or -f arguments.

Optional arguments (Must preceed required args):
-h                  Display this help and exit.
-t                  A table name or a file with a list of tables to backup.
-f                  A list of files to back up for this track set (/gbdb/ files).
-v                  Use a specified version string like "v1" instead of the output of 'date +%F'.
-s                  Print verbose status along the way to stderr.
-c                  Don't copy anything, just output what WOULD be copied

Backs up a list of tables or files for a track for a single database or list of
databases. Note the third required argument of what this track set is named. Exits 0 for success
and 1 on failure. The heirarchy created is:
\$archiveRoot/\$database/\$trackSetName/\$version/

Example Usages:
In the example below, a list of files (listOfBigBeds.txt) contains all the bigBeds for backup,
while we only want to back up one table, crisprRanges:
`basename $0` -t crisprRanges -f listOfBigBeds.txt /hive/data/inside/archive/ hg38 "CRISPR"

A list of tables would work similarly for the -t switch.
EOF
}

printVars() {
    printf "found variables:\n" 
    printf "archiveDir: '%s'\n" "${archiveDir}"
    printf "versionName: '%s'\n" "${versionName}"
    printf "dbs: '%s'\n" "${dbs}"
    printf "trackName: '%s'\n" "${trackName}"
    printf "tbls: '%s'\n" "${tables}"
    printf "files: '%s'\n" "${files}"
    exit 1
}

backupOneTable() {
    db=$1
    tbl=$2
    if [ ${verbose} = "TRUE" ]
    then
        printf "backing up %s\n" "$tbl" 1>&2
    fi
    set +e
    hgsql -Ne "select * from ${tbl}" ${db} > ${tbl}.txt
    if [ $? -ne 0 ]
    then
        set -e
        rm ${tbl}.txt
        printf "WARNING: back up of table '%s' failed. Ignore if only the trackDb entry is being backed up.\n" "${tbl}" 1>&2
    else
        set -e
        # -f forces an overwrite if you are re-running this script
        gzip -f ${tbl}.txt
        hgsql --raw -Ne "show create table ${tbl}" ${db} > ${tbl}.sql
    fi
    backupTrackDb ${db} ${tbl}
}

backupTables() {
    # dump tables to current directory
    db=$1
    tbls=$2
    if [ ${verbose} = "TRUE" ]
    then
        printf "got tables %s for db %s\n" "$tbls" "$db" 1>&2
    fi
    if [[ -e "${tbls}" ]]
    then
        for tbl in $(cat "${tbls}")
        do
            backupOneTable $db $tbl
        done
    else # may be a quoted list like "refGene mrna"
        for tbl in $(echo "${tbls}")
        do
            backupOneTable $db $tbl
        done
    fi
}

backupBigFiles() {
    # copy list of files in fname to current directory
    db=$1
    fname=$2
    if [ -e "${fname}" ]
    then
        # enforce fname is a list of files
        if [[ `file -L "${fname}" | cut -d' ' -f2-` = "ASCII text" ]]
        then
            for f in $(cat "${fname}")
            do
                if [ "${verbose}" = "TRUE" ]
                then
                    printf "backing up '%s'\n" 1>&2
                fi
                cp -p ${f} .
            done
        else
            printf "ERROR: '%s' is not a list of files\n" "${fname}" 1>&2
            EXIT_STATUS=1
        fi
    else
        printf "ERROR: '%s' does not exist\n" "${fname}" 1>&2
        EXIT_STATUS=1
    fi
}

backupTrackDb() {
    # if a table has a trackDb entry, back it up
    db=$1
    tbl=$2
    if [[ `hgsql ${db} -Ne "select count(*) from trackDb where tableName='${tbl}'"` -ne 0 ]]
    then
        if [ "${verbose}" = "TRUE" ]
        then
            printf "backing up trackDb for %s\n" "${tbl}" 1>&2
        fi
        set +e
        hgsql ${db} -Ne "select * from trackDb where tableName='${tbl}'" > ${tbl}.trackDb.tab
        if [ $? -ne 0 ]
        then
            set -e
            rm "${tbl}.trackDb.tab"
            printf "ERROR: backup of trackDb failed for %s\n" "${tbl}" 1>&2
            EXIT_STATUS=1
            exit 1
        else
            set -e
            # -f forces in case the script is being re-run
            gzip -f ${tbl}.trackDb.tab
        fi
    fi
}

doBackup()
{
    db=$1
    mkdir -p ${db}/"${trackSetName}"/"${versionName}"
    if [ "${verbose}" = "TRUE" ]
    then
        printf "created dir %s/%s/%s\n" "${db}" "${trackSetName}" "${versionName}"
    fi
    cd ${db}/"${trackSetName}"/${versionName}
    if [[ ! -z "${files}" ]]
    then
        backupBigFiles "${db}" "${files}"
    fi
    if [[ ! -z "${tables}" ]]
    then
        backupTables "${db}" "${tables}"
    fi
    cd ${archiveDir}
}

printCheck()
{
    currentDb=$1
    archRoot="`realpath ${archiveDir}`/${currentDb}/${trackSetName}/${versionName}"
    printf "check mode: moving files to the following directory:\n"
    printf "%s\n" "${archRoot}"
    printf "\n"
    if [[ -e "${tables}" ]]
    then
        printf "The following tables and trackDb's will be dumped:\n"
        for tbl in $(cat "${tables}")
        do
            printf "${archRoot}/${tbl}.gz\n" ""
            printf "${archRoot}/${tbl}.trackDb.tab.sql\n" ""
            printf "${archRoot}/${tbl}.sql\n" ""
        done
    else
        printf "The following tables and trackDb's will be dumped:\n"
        for tbl in ${tables}
        do
            printf "${archRoot}/${tbl}.gz\n" ""
            printf "${archRoot}/${tbl}.trackDb.tab.gz\n" ""
            printf "${archRoot}/${tbl}.sql\n" ""
        done
    fi
    printf "\n"
    if [[ -e "${files}" ]]
    then
        printf "The following big data files will be archived:\n"
        for f in $(cat "${files}")
        do
            fname=`basename ${f}`
            printf "%s/%s\n" "${archRoot}" "${fname}"
        done
    else
        printf "The following big data files will be archived:\n"
        for f in ${files}
        do
            fname=`basename ${f}`
            printf "%s/%s\n" "${archRoot}" "${fname}"
        done
    fi
}

##### Parse command-line input #####

#OPTIND=1 # Reset is necessary if getopts was used previously in the script.  It is a good idea to make this local in a function.
while getopts "hsct:f:v:" opt
do
    case $opt in
        h)
            usage
            exit 0
            ;;
        s)
            verbose="TRUE"
            ;;
        c)
            checkOnly="TRUE"
            ;;
        t)
            tables="${OPTARG}"
            ;;
        f)
            files="${OPTARG}"
            ;;
        v)
            versionName="${OPTARG}"
            ;;
        '?')
            printf "unknown option %s\n" "$opt" 1>&2
            usage >&2
            exit 1
            ;;
    esac
done

shift "$((OPTIND-1))" # Shift off the options and optional --.

# Check number of required arguments
if [[ $# -eq 0 ]]
then
    usage
    exit 0
fi

if [[ $# -ne 3 ]]
then
    # Output usage message if number of required arguments is wrong
    printf "missing required arguments\n"
    usage >&2
    exit 1
else
    # Set variables with required argument values
    archiveDir=$1
    dbList=$2
    trackSetName=$3
fi

# set the version string
if [[ -z "${versionName}" ]]
then
    versionName=`date +%F`
fi

# set the tables and files strings. If they are files with relative locations,
# prefix them with a full path so later functions work
if [[ -e "${files}" ]] && [[ "${files}" != /* ]]
then
    tmp="${files}"
    files=`pwd`/"${tmp}"
    if [ "${verbose}" = "TRUE" ]
    then
        printf "Fixing up files list to %s\n" "${files}"
    fi
fi

if [[ -e "${tables}" ]] && [[ "${tables}" != /* ]]
then
    tmp="${tables}"
    tables=`pwd`/"${tmp}"
    if [ "${verbose}" = "TRUE" ]
    then
        printf "Fixing up tables list to %s\n" "${tables}"
    fi
fi

# set the archive parent directory location
if [[ ! -d "${archiveDir}" ]]
then
    if [ "${verbose}" = "TRUE" ]
    then
        printf "Archiving to %s\n" "${archiveDir}" 1>&2
    fi
    if [ "${checkOnly}" != "TRUE" ]
    then
        mkdir -p "${archiveDir}"
    fi
fi

if [[ "${archiveDir}" != /* ]]
then
    tmp="${archiveDir}"
    archiveDir=`pwd`/"${tmp}"
fi


cd "${archiveDir}"
if [[ -f "${dbList}" ]]
then
    for db in $(cat "${dbList}")
    do
        if [ "${checkOnly}" = "TRUE" ]
        then
            printCheck ${db}
        else
            doBackup ${db}
        fi
    done
else
    for db in $(echo "${dbList}")
    do
        if [ "${checkOnly}" = "TRUE" ]
        then
            printCheck ${db}
        else
            doBackup ${db}
        fi
    done
fi
exit ${EXIT_STATUS}
