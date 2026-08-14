#!/bin/bash
#
# Parse the unzipped UMD TP53 TSVs in $DLDIR, build hg19 + hg38 bigBeds, run
# the trix index, sanity-check row counts against the previous release, and
# install symlinks into /gbdb on success.
#
# Argument 1: dated workdir from download.sh that holds UMD_variants_US.tsv
# and UMD_mutations_US.tsv.
#
# Do not modify this script in /hive/data/outside/otto/umdTp53; edit the kent
# source at src/hg/utils/otto/umdTp53/checkAndLoad.sh and `make install`.

set -o errexit -o pipefail -o nounset
umask 002

WORKDIR="/hive/data/outside/otto/umdTp53"
KENTBIN=/cluster/bin/x86_64

DLDIR="${1:?usage: checkAndLoad.sh <dated-workdir>}"
cd "${DLDIR}"

AS="${WORKDIR}/umdTp53.as"
PARSER="${WORKDIR}/umdTp53ToBed.py"

# 1. Build per-assembly BEDs.
python3 "${PARSER}" \
    --variants UMD_variants_US.tsv \
    --mutations UMD_mutations_US.tsv \
    --out-hg19 umdTp53.hg19.bed \
    --out-hg38 umdTp53.hg38.bed

# 2. Build the trix search input: one line per variant, `<id><TAB><terms>`.
#    Searchable terms: the cDNA name and the canonical protein change
#    (e.g. `c.524G>A` and `p.R175H` both find the R175H row).
#    Column 4 = BED name, column 11 = proteinChange (column 10 is cDnaFull).
awk 'BEGIN{FS=OFS="\t"} { terms = $4; if ($11 != "") terms = terms " " $11; print $4 "\t" terms }' \
    umdTp53.hg38.bed | sort -u > umdTp53.ix.in

for db in hg19 hg38; do
    bed="umdTp53.${db}.bed"
    sortedBed="umdTp53.${db}.sorted.bed"
    bb="umdTp53.${db}.bb"

    # 3. Validate row count: compare against the last shipped bigBed, refuse
    #    on a >10% swing in either direction (lovd / clinvar convention).
    new_rows=$(wc -l < "${bed}")
    prev_bb="${WORKDIR}/release/${db}/umdTp53.bb"
    if [ -f "${prev_bb}" ]; then
        old_rows=$("${KENTBIN}/bigBedInfo" "${prev_bb}" | awk '/itemCount:/ { gsub(",", "", $2); print $2 }')
        if [ "${old_rows}" -gt 0 ]; then
            awk -v old="${old_rows}" -v new="${new_rows}" -v db="${db}" '
                BEGIN {
                    delta = (new - old) / old
                    if (delta > 0.1 || delta < -0.1) {
                        printf "ERROR: umdTp53 %s row count moved %d -> %d (%.1f%%); refusing to install\n",
                               db, old, new, delta * 100 > "/dev/stderr"
                        exit 1
                    }
                }
            '
        fi
    fi
    if [ "${new_rows}" -lt 5000 ]; then
        echo "ERROR: umdTp53 ${db} only ${new_rows} rows; refusing to install" 1>&2
        exit 1
    fi

    # 4. Sort and build bigBed.
    sort -k1,1 -k2,2n "${bed}" > "${sortedBed}"
    "${KENTBIN}/bedToBigBed" -tab -type=bed9+ -as="${AS}" \
        -extraIndex=name "${sortedBed}" \
        /cluster/data/${db}/chrom.sizes "${bb}"
done

# 5. Build trix index (same .ix/.ixx for both assemblies — the variant names
#    are identical across hg19/hg38 builds).
"${KENTBIN}/ixIxx" umdTp53.ix.in umdTp53.ix umdTp53.ixx

# 6. Promote to release/ and refresh /gbdb symlinks.
for db in hg19 hg38; do
    mkdir -p "${WORKDIR}/release/${db}"
    cp "umdTp53.${db}.bb" "${WORKDIR}/release/${db}/umdTp53.bb"
    cp umdTp53.ix "${WORKDIR}/release/${db}/umdTp53.ix"
    cp umdTp53.ixx "${WORKDIR}/release/${db}/umdTp53.ixx"

    gbdir="/gbdb/${db}/bbi/umdTp53"
    mkdir -p "${gbdir}"
    ln -sfn "${WORKDIR}/release/${db}/umdTp53.bb"  "${gbdir}/umdTp53.bb"
    ln -sfn "${WORKDIR}/release/${db}/umdTp53.ix"  "${gbdir}/umdTp53.ix"
    ln -sfn "${WORKDIR}/release/${db}/umdTp53.ixx" "${gbdir}/umdTp53.ixx"
done

# 7. Roll prevRun snapshot for diff visibility on the next run.
mkdir -p "${WORKDIR}/prevRun"
cp umdTp53.hg19.bed "${WORKDIR}/prevRun/"
cp umdTp53.hg38.bed "${WORKDIR}/prevRun/"
