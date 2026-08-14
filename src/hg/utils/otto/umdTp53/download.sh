#!/bin/bash
#
# Check p53.fr for a new release of the UMD TP53 database; download the
# variants + mutations TSV zips into a dated working dir if anything changed,
# and unzip them. Silent on no-op so the cron entry point can stay quiet.
#
# Exits 0 with no output if nothing changed.
# Exits 0 and prints the dated workdir to stdout if a new build is needed.
# Exits non-zero on any download/extraction failure.
#
# Do not modify this script in /hive/data/outside/otto/umdTp53; edit the kent
# source at src/hg/utils/otto/umdTp53/download.sh and `make install`.

set -o errexit -o pipefail -o nounset
umask 002

WORKDIR="/hive/data/outside/otto/umdTp53"
STATEDIR="${WORKDIR}/state"
mkdir -p "${STATEDIR}"

VARIANTS_URL="https://p53.fr/images/Database/UMD_variants_US.tsv.zip"
MUTATIONS_URL="https://p53.fr/images/Database/UMD_mutations_US.tsv.zip"

# Returns the server's Last-Modified header for the URL, or empty on failure.
get_last_modified() {
    curl -sIL --fail "$1" 2>/dev/null | tr -d '\r' | awk '/^[Ll]ast-[Mm]odified:/ { sub(/^[Ll]ast-[Mm]odified: */, ""); print; exit }'
}

variants_lm_new=$(get_last_modified "${VARIANTS_URL}" || true)
mutations_lm_new=$(get_last_modified "${MUTATIONS_URL}" || true)

variants_lm_old=""
mutations_lm_old=""
[ -f "${STATEDIR}/variants.last_modified" ] && variants_lm_old=$(cat "${STATEDIR}/variants.last_modified")
[ -f "${STATEDIR}/mutations.last_modified" ] && mutations_lm_old=$(cat "${STATEDIR}/mutations.last_modified")

# Fast path: if both Last-Modified strings match what we saw last time, exit silently.
if [ -n "${variants_lm_new}" ] && [ -n "${mutations_lm_new}" ] \
   && [ "${variants_lm_new}" = "${variants_lm_old}" ] \
   && [ "${mutations_lm_new}" = "${mutations_lm_old}" ]; then
    exit 0
fi

# Otherwise download and content-hash before deciding to rebuild.
today=$(date +%F)
DLDIR="${WORKDIR}/${today}"
mkdir -p "${DLDIR}"
cd "${DLDIR}"

# --silent suppresses progress; --fail makes curl exit non-zero on 4xx/5xx.
curl --silent --fail --output UMD_variants_US.tsv.zip "${VARIANTS_URL}" 1>&2
curl --silent --fail --output UMD_mutations_US.tsv.zip "${MUTATIONS_URL}" 1>&2

unzip -oq UMD_variants_US.tsv.zip 1>&2
unzip -oq UMD_mutations_US.tsv.zip 1>&2

# Strip macOS resource-fork artifacts that ship inside the zips
rm -f ._UMD_*.tsv

# Compute MD5 of the unzipped TSVs; if both match the stored hashes, nothing
# actually changed (server-side metadata noise only).
variants_md5_new=$(md5sum UMD_variants_US.tsv | awk '{print $1}')
mutations_md5_new=$(md5sum UMD_mutations_US.tsv | awk '{print $1}')

variants_md5_old=""
mutations_md5_old=""
[ -f "${STATEDIR}/variants.md5" ] && variants_md5_old=$(cat "${STATEDIR}/variants.md5")
[ -f "${STATEDIR}/mutations.md5" ] && mutations_md5_old=$(cat "${STATEDIR}/mutations.md5")

if [ "${variants_md5_new}" = "${variants_md5_old}" ] \
   && [ "${mutations_md5_new}" = "${mutations_md5_old}" ]; then
    # Update Last-Modified stamps so we don't redownload on every cron run, then exit silently.
    [ -n "${variants_lm_new}" ] && echo "${variants_lm_new}" > "${STATEDIR}/variants.last_modified"
    [ -n "${mutations_lm_new}" ] && echo "${mutations_lm_new}" > "${STATEDIR}/mutations.last_modified"
    # The dated workdir is now redundant; remove it so the dir doesn't accumulate dupes.
    cd "${WORKDIR}"
    rm -rf "${DLDIR}"
    exit 0
fi

# Real content change. Persist new state stamps and report the workdir for downstream steps.
echo "${variants_md5_new}" > "${STATEDIR}/variants.md5"
echo "${mutations_md5_new}" > "${STATEDIR}/mutations.md5"
[ -n "${variants_lm_new}" ] && echo "${variants_lm_new}" > "${STATEDIR}/variants.last_modified"
[ -n "${mutations_lm_new}" ] && echo "${mutations_lm_new}" > "${STATEDIR}/mutations.last_modified"

# Stdout = the workdir path. The caller redirects this into a marker file.
echo "${DLDIR}"
