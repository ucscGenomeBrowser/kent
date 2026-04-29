#!/bin/bash

#	Do not modify this script, modify the source tree copy:
#	src/hg/utils/otto/dbVar/checkDbVar.sh
#	This script is used via a cron job and kept in $HOME/bin/scripts/
#	The source tree copy is installed to $WORKDIR via the makefile
#	in the same directory (make install).

set -eEu -o pipefail
WORKDIR=$1

cleanUpOnError () {
    echo "dbVar build failed"
}

trap cleanUpOnError ERR
trap "cleanUpOnError; exit 1" SIGINT SIGTERM
umask 002

#	cron jobs need to ensure this is true

#	this is where we are going to work
if [ ! -d "${WORKDIR}" ]; then
    printf "ERROR in dbVar build, can not find the directory: %s\n" "${WORKDIR}"
    exit 255
fi

# the release directory, where gbdb symlinks will point
if [ ! -d release ]; then
    mkdir -p ${WORKDIR}/release/{hg19,hg38}
fi

cd "${WORKDIR}"

# check if genome in a bottle variants have updated:
./checkNstd175.sh ${WORKDIR}

# Maximum fractional change in itemCount before a .bb file is flagged as
# suspicious and we refuse to promote it into release/. Matches the tooMuch=0.10
# convention used by validateDecipher.sh / validateGwas.sh / validateISCA.sh.
tooMuch=0.10

# Minimum plausible .bb file size (bytes). Anything smaller likely means a
# truncated / failed download. Smallest real file in current release is
# ~90 KB (common_east_asian_only.bb), so 10 KB is a safe floor.
minBytes=10240

#	see if anything is changing, if so, download, build, and email notify
wget -q https://ftp.ncbi.nlm.nih.gov/pub/dbVar/sandbox/dbvarhub/hub.txt -O tempUpdate
if [[ ! -e lastUpdate || tempUpdate -nt lastUpdate ]]; then
    today=`date +%F`
    mkdir -p $today
    cd $today
    hubClone -download https://ftp.ncbi.nlm.nih.gov/pub/dbVar/sandbox/dbvarhub/hub.txt

    # Stage the .bb files we expose via trackDb into release/${db}.new/ rather
    # than overwriting release/${db}/ in place. Validation (below) then decides
    # whether to promote .new/ to live via an atomic directory swap. This keeps
    # in-flight readers on the live files consistent and gives us a one-cycle
    # rollback copy in release/${db}.prev/.
    #
    # We mirror common_*.bb, conflict_*.bb, somatic_*.bb, and normal_*.bb.
    # clinvar_*.bb are intentionally skipped -- those are redundant with our
    # clinvarCnv track. Any new filename NCBI adds will trigger a notification
    # below (see knownFiles.txt diff) so we can decide whether to expose it.
    for db in hg19 hg38; do
        rm -rf ../release/${db}.new
        mkdir -p ../release/${db}.new
        cp dbVar/${db}/common_*.bb    ../release/${db}.new/
        cp dbVar/${db}/conflict_*.bb  ../release/${db}.new/
        cp dbVar/${db}/somatic_*.bb   ../release/${db}.new/
        cp dbVar/${db}/normal_*.bb    ../release/${db}.new/
    done
    cd ..

    # Validate each staged .bb file against the current live copy. Two gates:
    #   1) byte-size floor: catches truncated downloads.
    #   2) item-count delta: catches the case where NCBI ships a well-formed
    #      but mostly-empty file, or a 10x inflation from a build bug. The
    #      0.10 threshold matches the other UCSC otto validators.
    # On any failure we leave release/${db}.new/ in place for human inspection
    # and exit 1 so the wrapper emails the diagnostic.
    validationErrors=""
    for db in hg19 hg38; do
        for newFile in release/${db}.new/*.bb; do
            base=$(basename "$newFile")
            liveFile="release/${db}/${base}"

            newSize=$(stat -c%s "$newFile")
            if [ "$newSize" -lt "$minBytes" ]; then
                validationErrors+="  ${db}/${base}: new file only ${newSize} bytes (< ${minBytes}) -- likely truncated download\n"
                continue
            fi

            # First-time file (no prior live copy): accept without delta check.
            if [ ! -e "$liveFile" ]; then
                continue
            fi

            newCount=$(bigBedInfo "$newFile"  | awk '/^itemCount:/ {gsub(",","",$2); print $2}')
            oldCount=$(bigBedInfo "$liveFile" | awk '/^itemCount:/ {gsub(",","",$2); print $2}')
            if [ "$oldCount" -eq 0 ]; then continue; fi

            # |newCount - oldCount| / oldCount  > tooMuch  => fail
            tooMany=$(awk -v n="$newCount" -v o="$oldCount" -v t="$tooMuch" \
                      'BEGIN { d = n > o ? n - o : o - n; print (d / o > t) ? 1 : 0 }')
            if [ "$tooMany" = "1" ]; then
                validationErrors+="  ${db}/${base}: itemCount changed old=${oldCount} new=${newCount} (> ${tooMuch})\n"
            fi
        done
    done

    if [ -n "$validationErrors" ]; then
        printf "dbVar hub update: %s\n" "$(date)"
        printf "Source: https://ftp.ncbi.nlm.nih.gov/pub/dbVar/sandbox/dbvarhub/\n"
        printf "\n*** VALIDATION FAILED -- manual intervention required ***\n"
        printf "%b" "$validationErrors"
        printf "\nNew files are staged at ${WORKDIR}/release/{hg19,hg38}.new/.\n"
        printf "Inspect vs the live release/{hg19,hg38}/ copies, then either:\n"
        printf "  - rsync each .new/ over its live dir to accept, or\n"
        printf "  - rm -rf release/{hg19,hg38}.new to reject (next cron re-downloads).\n"
        printf "lastUpdate was NOT bumped; the next cron run will retry the fetch.\n"
        exit 1
    fi

    # Validation passed -- promote .new/ to live via directory rename. Per-file
    # mv would give smaller inconsistency windows but per-directory mv on the
    # same filesystem is already effectively atomic, and keeping the rollback
    # as a whole-directory snapshot is simpler to reason about.
    for db in hg19 hg38; do
        rm -rf release/${db}.prev
        [ -d release/${db} ] && mv release/${db} release/${db}.prev
        mv release/${db}.new release/${db}
    done

    # Detect new .bb files that NCBI has added to the hub since the last
    # acknowledged state (stored in knownFiles.txt). Union both assemblies
    # so a file added to only one assembly still gets flagged. If any are
    # found, the email should include the list so a human can decide whether
    # to add a trackDb stanza + gbdb symlink.
    # Use find rather than `ls *.bb` so a partial hub state (one assembly
    # missing files) doesn't blow up the script via set -e after release/
    # has already been promoted.
    currentFiles=$(find ${today}/dbVar/hg19 ${today}/dbVar/hg38 -maxdepth 1 -name '*.bb' -printf '%f\n' | sort -u)
    newFiles=$(comm -23 <(echo "$currentFiles") <(grep -v '^#' ${WORKDIR}/knownFiles.txt | grep -v '^$' | sort -u))

    # Print the email body: what ran, any new files, and a reminder of next
    # steps if there are additions. Everything below this point goes into
    # the email via the wrapper.
    printf "dbVar hub update: %s\n" "$(date)"
    printf "Source: https://ftp.ncbi.nlm.nih.gov/pub/dbVar/sandbox/dbvarhub/\n"
    printf "Validation: all files passed size + 10%% itemCount delta checks.\n"
    if [ -n "$newFiles" ]; then
        printf "\n*** NEW FILES in NCBI hub (not in knownFiles.txt) ***\n"
        printf "%s\n" "$newFiles"
        printf "\nUpdate trackDb + /gbdb symlinks if these should be exposed,\n"
        printf "then update src/hg/utils/otto/dbVar/knownFiles.txt and run\n"
        printf "'make install' in that directory to acknowledge them.\n"
    fi

    mv tempUpdate lastUpdate
else
    # No hub update -- stay silent so the wrapper doesn't email.
    rm tempUpdate
fi
