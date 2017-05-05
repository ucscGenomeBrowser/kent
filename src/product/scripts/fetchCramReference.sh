#!/bin/bash
set -beEu -o pipefail
umask 002

# Given two directories: one containing files with URLs of files that need
# to be downloaded and the other as a destination for those files,
# perform the downloads.  Locks are used to prevent multiple instances
# from colliding.  On error, an error file is generated and the lock is left
# in place.  User intervention is required to then resolve the issue.

export MAXTRIES=5

usage() {
    echo "fetchCramReference.sh - fetch reference sequence files from remote servers"
    echo "Usage:"
    echo -e "\tfetchCramReference.sh <pending> <destination> <error>"
    echo "Where: <pending>, <destination>, and <error> are all directories."
    echo "This program provides automated download of remote files."
    echo "The files in the pending directory should contain the URLs"
    echo "of files to be downloaded - one per file.  The downloads"
    echo "will be placed in the destination directory using the name"
    echo "the file possessed in the pending directory.  In the event of"
    echo "a problem, a related error message will be placed in the"
    echo "error directory using the same filename."
    exit 255
}


# rudimentary command-line handling

if [ $# -lt 3 ]
then
  usage
fi

export PENDING=$1
export DESTDIR=$2
export ERRORDIR=$3

if [ ! -d "$PENDING" ]
then
    echo "Error: $PENDING is missing or not a directory"
    exit 255
fi

if [ ! -d "$DESTDIR" ]
then
    echo "Error: $DESTDIR is missing or not a directory"
    exit 255
fi

if [ ! -d "$ERRORDIR" ]
then
    echo "Error: $ERRORDIR is missing or not a directory"
    exit 255
fi

if lockfile -! -r 0 "${PENDING}/fetch.lock" >& /dev/null
then
    # echo "${PENDING}/fetch.lock already exists.  Exiting."
    exit 0
fi


# set up cleanup in the event of Ctrl-C
trap '{rm -f "${PENDING}/*.out"; rm -f ${PENDING}/fetch.lock; exit 1; }' INT


# Re-add files that previously failed, if they failed long enough ago.
# Retry after 1 hour; give up after $MAXTRIES attempts
find ${ERRORDIR} -type f -mmin +60 -printf "%f\0" |
 xargs -0 -n 1 -I % sh -c \
'
    if [ -e "${PENDING}/%.try${MAXTRIES}" ]
    then
        rm -f "${PENDING}/%.try${MAXTRIES}" "${ERRORDIR}/%"
    else
        cp $(find "${PENDING}" -maxdepth 1 -type f -name "%.try*" -print -quit) "${PENDING}/%" || true
        rm -f "${ERRORDIR}/%"
    fi
'


# Fetch files in parallel; up to 5 at a time.  If xargs does not support -P 5
# in your Unix, delete that option to do serial fetch.

ls "$PENDING" | (egrep -v 'fetch.lock|.out|.try' || true) |
xargs -I % -n 1 -P 5 sh -c \
'
    if WGETOUT=$(wget -nv -i "${PENDING}/%" -O "${PENDING}/%.out" 2>&1)
    then
        # Download successful
        mv "${PENDING}/%.out" "${DESTDIR}/%" &&
        rm -f "${PENDING}/%" "${PENDING}/%.try*" ||
        echo "Internal server error: Please contact system administrator" > ${ERRORDIR}/%
    else
        # Download failed, record wget error message
        echo "$WGETOUT" | tr "\n" "\t" > "${ERRORDIR}/%"
    fi
    # On error, increment try count
    if [ -e "${ERRORDIR}/%" ]
    then
        OLDTRY=$(find "${PENDING}" -maxdepth 1 -type f -name "%.try*" -print -quit)
        if [[ "${OLDTRY}" == "" ]]
        then
            mv "${PENDING}/%" "${PENDING}/%.try1"
        else
            NEWTRY=$(echo "${OLDTRY}" | perl -pe "s/try(\d+)$/\"try\".(\$1+1)/e" )
            mv "${OLDTRY}" "${NEWTRY}"
            rm -f "${PENDING}/%"
        fi
        rm -f "${PENDING}/%.out"
    fi
'

rm -f "${PENDING}/fetch.lock"
