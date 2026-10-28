#!/bin/bash

#############################################################################
###  This source is from the source tree:
###     ~/kent/src/hg/utils/otto/genArk/updateTrackCounts.sh
###  do *not* edit this in the otto directory
###  /hive/data/inside/GenArk/pushRR/ where this is used.
###
###  Records "grep -c -w track" against each accession's public.hub.txt
###  (or hub.txt, if no public.hub.txt exists) -- a proxy for track count
###  per GenArk assembly -- without re-grepping every one of the ~52,000
###  accessions on every run.
###
###  Discovery reuses dev.todayList.gz, the dereferenced (stat -L) file
###  listing devList.sh already produces daily for the push pipeline --
###  no separate find/stat pass here.  public.hub.txt/hub.txt are
###  symlinks; grep follows them transparently when it opens the path,
###  but detecting whether one has *changed* needs the dereferenced
###  mtime, which is exactly what dev.todayList.gz already carries.
###
###  An accession is only (re-)grepped when it's new, or the mtime (or
###  the choice of public.hub.txt vs hub.txt) recorded in the cache
###  differs from today's snapshot.  Everything else is carried forward
###  from the previous cache untouched.
###
###  results cache: trackCounts/trackCounts.tsv
###    accession \t sourceFile \t mtime \t trackCount
#############################################################################

set -beEu -o pipefail

export expectHost="hgwdev"
export hostName=$(hostname -s)
if [[ "${hostName}" != "${expectHost}" ]]; then
  printf "ERROR: must run this on %s !  This is: %s\n" "${expectHost}" "${hostName}" 1>&2
  exit 255
fi

export TOP="/hive/data/inside/GenArk/pushRR"
export gbdbTop="/gbdb/genark"
export devList="${TOP}/dev.todayList.gz"
export outDir="${TOP}/trackCounts"
export cacheFile="${outDir}/trackCounts.tsv"

if [ ! -s "${devList}" ]; then
  printf "ERROR: %s not found -- devList.sh must run first\n" "${devList}" 1>&2
  exit 255
fi

mkdir -p "${outDir}"
touch "${cacheFile}"

export pid="$$"
export todaySnap="/dev/shm/trackCounts.today.${pid}.tsv"
export toProcess="/dev/shm/trackCounts.toProcess.${pid}.tsv"
export unchangedFile="/dev/shm/trackCounts.unchanged.${pid}.tsv"
export processedFile="/dev/shm/trackCounts.processed.${pid}.tsv"
export newCache="/dev/shm/trackCounts.newCache.${pid}.tsv"

##############################################################################
### step 1: today's snapshot of hub.txt / public.hub.txt, one line per
### accession, preferring public.hub.txt over hub.txt when both exist
##############################################################################
zcat "${devList}" | awk -F'\t' '
  {
    n = split($2, p, "/")
    file = p[n]
    if (file != "hub.txt" && file != "public.hub.txt") next
    acc = p[n-1]
    if (file == "public.hub.txt") {
      path[acc] = $2; mtime[acc] = $1; src[acc] = "public.hub.txt"
    } else if (!(acc in src)) {
      path[acc] = $2; mtime[acc] = $1; src[acc] = "hub.txt"
    }
  }
  END {
    for (a in path) printf "%s\t%s\t%s\t%s\n", a, src[a], mtime[a], path[a]
  }
' | sort -k1,1 > "${todaySnap}"

##############################################################################
### step 2: split into "unchanged" (same source+mtime as the cache --
### carry the old count forward, no grep needed) and "toProcess" (new
### accession, or source/mtime differs -- needs a fresh grep)
##############################################################################
awk -F'\t' -v cacheFile="${cacheFile}" -v toProcess="${toProcess}" \
    -v unchangedFile="${unchangedFile}" '
  BEGIN {
    while ((getline line < cacheFile) > 0) {
      split(line, f, "\t")
      cSrc[f[1]] = f[2]; cMtime[f[1]] = f[3]; cCount[f[1]] = f[4]
    }
    close(cacheFile)
  }
  {
    acc = $1; src = $2; mtime = $3; path = $4
    if ((acc in cSrc) && cSrc[acc] == src && cMtime[acc] == mtime) {
      printf "%s\t%s\t%s\t%s\n", acc, src, mtime, cCount[acc] >> unchangedFile
    } else {
      printf "%s\t%s\t%s\t%s\n", acc, src, mtime, path >> toProcess
    }
  }
' "${todaySnap}"

touch "${unchangedFile}" "${toProcess}" "${processedFile}"

##############################################################################
### step 3: grep only the new-or-changed accessions
##############################################################################
export errorCount=0
if [ -s "${toProcess}" ]; then
  while IFS=$'\t' read -r acc src mtime relPath
  do
    fullPath="${gbdbTop}/${relPath}"
    if [ ! -r "${fullPath}" ]; then
      printf "ERROR: cannot read %s for %s, will retry next run\n" "${fullPath}" "${acc}" 1>&2
      errorCount=$((errorCount + 1))
      continue
    fi
    count=$(grep -c -w track "${fullPath}" || true)
    printf "%s\t%s\t%s\t%s\n" "${acc}" "${src}" "${mtime}" "${count}" >> "${processedFile}"
  done < "${toProcess}"
fi

##############################################################################
### step 4: merge unchanged + freshly processed into the new cache, and
### swap it in atomically.  accessions that dropped out of today's
### snapshot entirely (removed assemblies) are naturally dropped here
### too, since they appear in neither file.
##############################################################################
cat "${unchangedFile}" "${processedFile}" | sort -k1,1 > "${newCache}"
mv "${newCache}" "${cacheFile}"

printf "### track counts updated %s\n" "$(date '+%F %T')"
printf "###   total accessions today: %d\n" "$(wc -l < "${todaySnap}")"
printf "###   unchanged (skipped):    %d\n" "$(wc -l < "${unchangedFile}")"
printf "###   processed (grepped):    %d\n" "$(wc -l < "${processedFile}")"
printf "###   errors (unreadable):    %d\n" "${errorCount}"

rm -f "${todaySnap}" "${toProcess}" "${unchangedFile}" "${processedFile}"
