#!/bin/bash

# measureHubSpace.sh - snapshot some stats about the state of the hubSpace upload area
#
# Runs on the hubspace machine (production) and on hgwdev (development copy).
# The only difference between the two is the tusd data directory, set in the
# case statement below, so this script can be copied between the two machines
# unchanged.  Logs land in ~/hubSpace/YYYY/MM/ on both.

set -beEu -o pipefail

export LC_ALL=C

usage() {
    printf "usage: measureHubSpace.dev.sh [--stdout]\n" 1>&2
    printf "    --stdout    write the report to stdout instead of the dated log file\n" 1>&2
}

toStdout="no"
if [ $# -gt 0 ]; then
    case "${1}" in
        --stdout) toStdout="yes" ;;
        -h|--help) usage; exit 0 ;;
        *) usage; exit 1 ;;
    esac
fi

hostname=$(hostname -s)
case "${hostname}" in
    hubspace) WORKDIR="/data/tusd/data" ;;
    hgwdev)   WORKDIR="/data/tusd" ;;
    *)
        printf "stopping because not on the hubspace or hgwdev machine (host: %s)\n" "${hostname}" 1>&2
        exit 1
        ;;
esac
LOGROOT="${HOME}/hubSpace"
# for testing the report against a synthetic tree
WORKDIR="${HUBSPACE_WORKDIR:-${WORKDIR}}"
export WORKDIR LOGROOT

export YYYY=`date "+%Y"`
export MM=`date "+%m"`
export DS=`date "+%Y-%m-%dT%H"`

LOGDIR="${LOGROOT}/${YYYY}/${MM}"
LOG="${LOGDIR}/${DS}.txt"
export LOGDIR LOG

if [ "${toStdout}" = "yes" ]; then
    LOG="/dev/stdout"
else
    mkdir -p ${LOGDIR}
fi

# One walk of the tree feeds every tally below: size, full path, basename.
# find exits non zero for anything it cannot read, and since -printf '%s' has to
# stat the file those entries are missing from the tallies entirely, so the
# errors are collected and reported rather than being allowed to kill the run.
FILELIST=$(mktemp -t hubSpaceFiles.XXXXXX)
UNKNOWN=$(mktemp -t hubSpaceUnknown.XXXXXX)
ERRLIST=$(mktemp -t hubSpaceErrors.XXXXXX)
SCRATCH=$(mktemp -t hubSpaceScratch.XXXXXX)
trap 'rm -f "${FILELIST}" "${UNKNOWN}" "${UNKNOWN}.shown" "${ERRLIST}" "${SCRATCH}"' EXIT

findStatus=0
find ${WORKDIR} -type f -printf '%s\t%p\t%f\n' > ${FILELIST} 2>> ${ERRLIST} || findStatus=$?
fileCount=$(wc -l < ${FILELIST})
if [ "${fileCount}" -eq 0 ] && [ "${findStatus}" -ne 0 ]; then
    printf "stopping because find %s failed and produced nothing:\n" "${WORKDIR}" 1>&2
    cat ${ERRLIST} 1>&2
    exit 1
fi

# shared awk helpers: extension classification and byte formatting.  Anything
# not in the known list below ends up in the "unrecognized" bucket and is
# listed out individually, instead of being silently counted as a file type.
AWK_LIB=$(cat <<'EOF'
BEGIN {
    FS = "\t"
    knownList = "bb bigbed bw bigwig bed bed.gz bedgraph bedgraph.gz wig " \
                "2bit fa fa.gz fasta fasta.gz " \
                "bam bai cram crai vcf vcf.gz tbi csi " \
                "hic cool mcool " \
                "txt html gff gff.gz gff3 gff3.gz gtf gtf.gz " \
                "psl bigpsl chain bigchain net maf bigmaf " \
                "genepred biggenepred narrowpeak broadpeak " \
                "bigbarchart barchart interact biginteract bignarrowpeak " \
                "bigmethyl bigdbsnp longtabix pgsnp vcftabix"
    split(knownList, kTmp, " ")
    for (i in kTmp)
        known[kTmp[i]] = 1
    # extensions that mean "this is a companion index, not track data"
    split("bai crai tbi csi", iTmp, " ")
    for (i in iTmp)
        isIndex[iTmp[i]] = 1
    # recognizable, but not something a hub can display: users dropping a whole
    # working directory in place bring along scripts, logs and intermediates
    auxList = "as log py sh bash pl r rmd ipynb json yaml yml md pdf png jpg " \
              "jpeg svg csv tsv tab xlsx docx gz zip tar tar.gz tgz " \
              "chain chain.gz net net.gz psl.gz maf.gz sam sizes err out"
    split(auxList, aTmp, " ")
    for (i in aTmp) {
        known[aTmp[i]] = 1
        isAux[aTmp[i]] = 1
    }
    # extensions are lower cased so that bigWig and bigwig tally together,
    # these are spelled back out the conventional way just for the report
    dispList = "bigbed:bigBed bigwig:bigWig bedgraph:bedGraph " \
               "bedgraph.gz:bedGraph.gz genepred:genePred biggenepred:bigGenePred " \
               "narrowpeak:narrowPeak broadpeak:broadPeak bignarrowpeak:bigNarrowPeak " \
               "bigpsl:bigPsl bigchain:bigChain bigmaf:bigMaf bigbarchart:bigBarChart " \
               "barchart:barChart biginteract:bigInteract bigmethyl:bigMethyl " \
               "bigdbsnp:bigDbSnp vcftabix:vcfTabix longtabix:longTabix pgsnp:pgSnp"
    split(dispList, dTmp, " ")
    for (i in dTmp) {
        split(dTmp[i], pair, ":")
        display[pair[1]] = pair[2]
    }
}

function displayType(type) {
    if (type in display)
        return display[type]
    return type
}

# the type of a file, taken from its basename (never from its path depth)
function classify(name,   n, parts, lext) {
    if (name ~ /^\./)                       # .hub.lock and friends
        return name
    n = split(name, parts, ".")
    if (n < 2 || parts[n] == "")
        return "(no extension)"
    lext = tolower(parts[n])
    # keep the compression suffix attached: foo.vcf.gz is a vcf, not a gz
    if (lext ~ /^(gz|bgz|bz2|xz|zst)$/ && n > 2 && parts[n-1] != "")
        return tolower(parts[n-1]) "." lext
    return lext
}

# which bucket a type belongs to in the grouped summary
function category(type) {
    if (type ~ /^\./)
        return "internal"
    if (type == "(no extension)")
        return "noExtension"
    if (type == "info")                     # tusd's own per upload metadata
        return "tusdMeta"
    if (!(type in known))
        return "unrecognized"
    if (type in isIndex)
        return "index"
    if (type in isAux)
        return "auxiliary"
    if (type == "txt")
        return "hubText"
    return "trackData"
}

function human(b,   u, i) {
    split("B K M G T P", u, " ")
    i = 1
    while (b >= 1024 && i < 6) {
        b /= 1024
        i++
    }
    if (i == 1)
        return sprintf("%d%s", b, u[i])
    return sprintf("%.1f%s", b, u[i])
}
EOF
)
export AWK_LIB

# anything unreadable is called out first, since it means the tallies below are
# incomplete.  A hub directory without its x bit is the usual cause: its entries
# can be listed but not stat'ed by anyone other than the owner, so find skips
# them.  Every hub directory is normally mode 0777 apache:apache.
printf "# paths that could not be read\n" >> ${LOG}
errCount=$(wc -l < ${ERRLIST})
printf "# count: %d\n" "${errCount}" >> ${LOG}
if [ "${errCount}" -gt 0 ]; then
    sed -e "s#${WORKDIR}/##" ${ERRLIST} >> ${LOG}
fi

# directories a non owner cannot search, ie the reason for the errors above.
# The mode is reported because find cannot descend to count what is inside.
printf "# directories not searchable by other (contents cannot be stat'ed)\n" >> ${LOG}
find ${WORKDIR} -mindepth 1 -type d ! -perm -o+x -printf '%M %u:%g %p\n' 2> /dev/null \
    | sed -e "s#${WORKDIR}/##" > ${SCRATCH} || true
printf "# count: %d\n" "$(wc -l < ${SCRATCH})" >> ${LOG}
cat ${SCRATCH} >> ${LOG}

# leading or trailing whitespace in a name is legal but almost always a mistake,
# and it is invisible in a directory listing
printf "# names with leading or trailing whitespace\n" >> ${LOG}
find ${WORKDIR} -mindepth 1 \( -name ' *' -o -name '* ' -o -name '*	*' \) \
    -printf '%y %p\n' 2> /dev/null | sed -e "s#${WORKDIR}/##" > ${SCRATCH} || true
printf "# count: %d\n" "$(wc -l < ${SCRATCH})" >> ${LOG}
cat ${SCRATCH} >> ${LOG}

# tally the total number of uploaded files
printf "# total number of uploaded files\n" >> ${LOG}
printf "%d\n" "${fileCount}" >> ${LOG}

# tally the total disk space
printf "# total disk space consumed\n" >> ${LOG}
du -sch ${WORKDIR} 2>> /dev/null >> ${LOG} || true

# tally the file type counts based on the extension of the file name itself.
# The raw byte total is carried through as the first column just to sort on it,
# largest first, then dropped from the report.
printf "# file type counts\n" >> ${LOG}
printf "#   count       bytes  fileType\n" >> ${LOG}
awk "${AWK_LIB}"'
{
    type = classify($3)
    count[type]++
    bytes[type] += $1
}
END {
    for (t in count)
        printf "%.0f\t%d\t%s\t%s\n", bytes[t], count[t], human(bytes[t]), displayType(t)
}' ${FILELIST} \
    | sort -t$'\t' -k1,1nr \
    | awk -F'\t' '{ printf "%8d  %10s  %s\n", $2, $3, $4 }' >> ${LOG}

# roll the extensions up into categories, so cruft is easy to spot
printf "# file type counts grouped by category\n" >> ${LOG}
printf "#   count       bytes  category\n" >> ${LOG}
awk "${AWK_LIB}"'
{
    cat = category(classify($3))
    count[cat]++
    bytes[cat] += $1
}
END {
    for (c in count)
        printf "%.0f\t%d\t%s\t%s\n", bytes[c], count[c], human(bytes[c]), c
}' ${FILELIST} \
    | sort -t$'\t' -k1,1nr \
    | awk -F'\t' '{ printf "%8d  %10s  %s\n", $2, $3, $4 }' >> ${LOG}

# The hub text files are mostly the handful of names hubSpace generates itself,
# so only the repeated names are listed.  The one off names are almost always a
# user's own data file, sometimes another format with .txt stuck on the end.
printf "# hub text file names seen more than once\n" >> ${LOG}
awk "${AWK_LIB}"'
classify($3) == "txt" { count[$3]++ }
END {
    for (n in count)
        if (count[n] > 1)
            printf "%8d  %s\n", count[n], n
}' ${FILELIST} | sort -rn >> ${LOG}
# kept out of the sort above so it stays at the end of the list
awk "${AWK_LIB}"'
classify($3) == "txt" { count[$3]++ }
END {
    onceCount = 0
    for (n in count)
        if (count[n] == 1)
            onceCount++
    printf "%8d  # more names seen only once, not listed\n", onceCount
}' ${FILELIST} >> ${LOG}

# text files named as though they hold some other format, eg foo.bed.txt
printf "# text files whose name suggests another format\n" >> ${LOG}
awk "${AWK_LIB}"'
{
    if (classify($3) != "txt")
        next
    n = split(tolower($3), parts, ".")
    if (n > 2 && parts[n-1] in known && parts[n-1] != "txt")
        count[parts[n-1]]++
}
END {
    for (t in count)
        printf "%8d  .%s.txt\n", count[t], t
}' ${FILELIST} | sort -rn >> ${LOG}

# file(1) knows nothing about the UCSC binary formats and just calls them
# "data", which is no help at all for a file with no extension.  These are the
# magic numbers from the front of each format, either byte order.
sniffMagic() {
    local path="${1}" magic="" desc=""
    magic=$(od -An -tx1 -N 8 "${path}" 2> /dev/null | tr -d ' \n' || true)
    case "${magic}" in
        26fc8f88*|888ffc26*) printf "bigWig"; return ;;
        ebf28987*|8789f2eb*) printf "bigBed"; return ;;
        43274a1a*|1a412743*) printf "2bit"; return ;;
        424d0100*)           printf "bam"; return ;;
        48494301*)           printf "hic"; return ;;
        "")                  printf "unreadable or empty"; return ;;
    esac
    # not one of ours, file(1) is better at the rest.  Its output runs long for
    # compressed data, and only the front of it is ever interesting here.
    desc=$(file -b "${path}" 2> /dev/null || true)
    if [ -z "${desc}" ]; then
        desc="unknown magic ${magic:0:8}"
    fi
    printf "%.60s" "${desc}"
}

# List out the files that have no extension or an extension we do not know,
# these are the ones showing up with no useful file type in the counts above.
# Only a few examples of each type are listed, because a single user unpacking a
# working directory into a hub can otherwise fill the whole report.
export MAXPERTYPE=4
printf "# files with no extension or an unrecognized extension\n" >> ${LOG}
awk -v workdir="${WORKDIR}" "${AWK_LIB}"'
{
    type = classify($3)
    cat = category(type)
    if (cat != "noExtension" && cat != "unrecognized")
        next
    # the full path is needed to read the file, the trimmed one is what we report
    relPath = $2
    if (substr(relPath, 1, length(workdir) + 1) == workdir "/")
        relPath = substr(relPath, length(workdir) + 2)
    printf "%s\t%s\t%s\t%s\n", type, $1, $2, relPath
}' ${FILELIST} > ${UNKNOWN}

unknownCount=$(wc -l < ${UNKNOWN})
printf "# count: %d, showing at most %d of each type, largest first\n" \
    "${unknownCount}" "${MAXPERTYPE}" >> ${LOG}
if [ "${unknownCount}" -gt 0 ]; then
    printf "#      bytes  path  [content]\n" >> ${LOG}
    sort -t$'\t' -k1,1 -k2,2nr ${UNKNOWN} \
        | awk -F'\t' -v maxPer="${MAXPERTYPE}" '
            { shown[$1]++ }
            shown[$1] <= maxPer
            END {
                for (t in shown)
                    if (shown[t] > maxPer)
                        printf "#\t%d\t%s\n", shown[t] - maxPer, t
            }' > ${UNKNOWN}.shown
    while IFS=$'\t' read -r type fileSize filePath relPath; do
        if [ "${type}" = "#" ]; then
            printf "#   ... %d more of type %s not listed\n" "${fileSize}" "${filePath}" >> ${LOG}
            continue
        fi
        printf "%11d  %s  [%s]\n" "${fileSize}" "${relPath}" "$(sniffMagic "${filePath}")" >> ${LOG}
    done < ${UNKNOWN}.shown
    rm -f ${UNKNOWN}.shown
fi

# tally the users
printf "# userNames of uploaders\n" >> ${LOG}
find ${WORKDIR} -mindepth 2 -maxdepth 2 -type d -printf '%f\n' 2>> /dev/null \
    | sort >> ${LOG} || true
