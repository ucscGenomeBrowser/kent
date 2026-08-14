#!/bin/bash
#
# genarkContrib.sh — publish a contributor's tracks into the GenArk build tree.
#
# Usage: genarkContrib.sh <contribName>
#
# Run from /hive/data/outside/genark/<contribName>/, which must contain
# per-assembly subdirectories named after their NCBI accession (GCA_* or GCF_*).
#
# For every assembly directory matching ./GC[AF]_*/ under the contributor's tree,
# this script:
#   1. Locates the corresponding GenArk build directory under
#      /hive/data/genomes/asmHubs/{genbankBuild,refseqBuild}/...
#   2. Creates  <buildDir>/contrib/<contribName>/  if needed.
#   3. Symlinks every non-trackDb file from the local assembly dir into that
#      contrib subdir (so .bb, .html, etc. are served from the build tree
#      without duplicating data).
#   4. Rewrites the assembly's trackDb file so that searchTrix / bigDataUrl /
#      html paths point at  contrib/<contribName>/<file>,  and writes the
#      result as  <contribName>.trackDb.txt  next to the symlinks.
#
# Re-running is safe: existing symlinks are removed and recreated.

set -u

# --- Arguments --------------------------------------------------------------

if [ "$#" -ne 1 ] || [ -z "${1:-}" ]; then
    printf "Usage: %s <contribName>\n" "$(basename "$0")" 1>&2
    exit 1
fi

contribName="$1"                                  # subdir name under each build's contrib/
TOP="/hive/data/outside/genark/${contribName}"    # root of this contributor's tree
asmHubsRoot="/hive/data/genomes/asmHubs"          # where GenArk builds live

if [ ! -d "$TOP" ]; then
    printf "ERROR: contributor tree not found: %s\n" "$TOP" 1>&2
    exit 1
fi

# --- Helpers ----------------------------------------------------------------

# Convert an NCBI assembly accession (e.g. GCA_000002325.2) into the
# 3-3-3 GenArk path layout (e.g. GCA/000/002/325/GCA_000002325.2).
accessionToGenArkPath() {
    local acc="$1"
    local prefix="${acc:0:3}"     # GCA or GCF
    local d0="${acc:4:3}"
    local d1="${acc:7:3}"
    local d2="${acc:10:3}"
    printf "%s/%s/%s/%s/%s" "$prefix" "$d0" "$d1" "$d2" "$acc"
}

# Pick the build subtree: GCF -> refseqBuild, GCA -> genbankBuild.
buildSubtreeFor() {
    case "${1:0:3}" in
        GCF) echo "refseqBuild" ;;
        *)   echo "genbankBuild" ;;
    esac
}

# --- Main loop --------------------------------------------------------------

ls -d */GC* 2>/dev/null | while read -r assemblyDir; do
    [ -d "$assemblyDir" ] || continue

    acc=$(basename "$assemblyDir")
    relPath=$(accessionToGenArkPath "$acc")
    subtree=$(buildSubtreeFor "$acc")

    # The accession on disk may carry a version suffix; glob to tolerate it.
    buildPath=$(ls -d "${asmHubsRoot}/${subtree}/${relPath}"* 2>/dev/null)

    if [ ! -d "$buildPath" ]; then
        printf "ERROR: no GenArk build found for %s (looked in %s/%s*)\n" \
            "$acc" "${asmHubsRoot}/${subtree}" "$relPath" 1>&2
        continue
    fi

    contribDir="${buildPath}/contrib/${contribName}"
    mkdir -p "$contribDir"

    # 1. Symlink every non-trackDb file into contrib/<contribName>/.
    find "./${assemblyDir}" -type f ! -name '*trackDb*' \
        | sed -e 's#^\./##' \
        | while read -r relFile; do
            target="${TOP}/${relFile}"
            linkPath="${contribDir}/$(basename "$relFile")"
            rm -f "$linkPath"
            ln -s "$target" "$linkPath"
            printf "ln -s %s %s\n" "$target" "$linkPath"
          done

    # 2. Rewrite trackDb so paths resolve under contrib/<contribName>/.
    find "./${assemblyDir}" -type f -name '*trackDb*' \
        | sed -e 's#^\./##' \
        | while read -r tdb; do
            sed -e "s#^searchTrix[[:space:]]\+#searchTrix contrib/${contribName}/#" \
                -e "s#^bigDataUrl[[:space:]]\+#bigDataUrl contrib/${contribName}/#" \
                -e "s#^html[[:space:]]\+#html contrib/${contribName}/#" \
                "$tdb" > "${contribDir}/${contribName}.trackDb.txt"
            printf "rewrote %s -> %s/%s.trackDb.txt\n" \
                "$tdb" "$contribDir" "$contribName"
          done
done
