#!/bin/bash
# Orchestrate the whole HPRC Release 2 contributed-track hub build.
# Builds every track for every assembly with GNU parallel (default -j 20),
# then generates genomes.txt and per-assembly trackDb.txt.
#
# Usage: hprc2annotBuild.sh [JOBS] [TRACK ...]
#   JOBS   parallel width, default 20
#   TRACK  restrict the run to these tracks, default all seven
# Re-run safe: an (assembly,track) whose output already exists and is a readable
# bigBed/bigWig is skipped. Set HPRC2_FORCE=1 to rebuild those too, which is what
# you want after changing how a track is converted.
# Examples:
#   hprc2annotBuild.sh 20                        # fill in whatever is missing
#   HPRC2_FORCE=1 hprc2annotBuild.sh 20 segdups  # rebuild every segdups file
set -eu

JOBS=${1:-20}
shift || true
WORK=/hive/data/genomes/asmHubs/contrib/hprc2annot.build
HUB=/hive/data/genomes/asmHubs/contrib/hprc2annot
SCR=$HOME/kent/src/hg/makeDb/scripts/hprc2annot
IDX=$WORK/idx
ALLTRACKS="cat liftoff censat censatCen pclai segdups methyl"
TRACKS=${*:-$ALLTRACKS}
FORCE=${HPRC2_FORCE:-0}
for t in $TRACKS; do
  case " $ALLTRACKS " in *" $t "*) ;; *) echo "unknown track: $t" >&2; exit 2;; esac
done
echo "tracks: $TRACKS   jobs: $JOBS   force: $FORCE"

# HPRC index CSVs arrive with CRLF line endings; a trailing \r on the S3 URL
# field makes curl reject it (error 3). Normalise to LF up front.
for f in $IDX/*.csv; do sed -i 's/\r$//' "$f"; done

# file each track writes into its assembly dir (to test for resume)
outfile() { case "$1" in
  cat) echo catGenes.bb;; liftoff) echo liftoffGenes.bb;;
  censat) echo censat.bb;; censatCen) echo censatCentromeres.bb;;
  pclai) echo pclai.bb;; segdups) echo segdups.bb;; methyl) echo methylation.bw;; esac; }

# valid-output check for resume: bigWig for .bw, bigBed for .bb
valid() { case "$1" in *.bw) bigWigInfo "$1" >/dev/null 2>&1;; *) bigBedInfo "$1" >/dev/null 2>&1;; esac; }

# (sample,hap) -> accession, from the assemblies index
declare -A ACC
while IFS=$'\t' read -r s h a; do ACC["$s|$h"]="$a"; done \
  < <(tail -n +2 $IDX/asm_index.csv | awk -F, 'BEGIN{OFS="\t"}{print $1,$2,$9}')

# assemblies_release2 has the two haplotype accessions SWAPPED for three samples
# (verified: the GenArk chromAlias hprcV2 column declares the opposite hap for
# these accessions). Correct the mapping so data is built against the assembly
# whose sequences actually match.
ACC["HG01978|1"]=GCA_018472865.2; ACC["HG01978|2"]=GCA_018472845.2
ACC["HG02257|1"]=GCA_018466845.2; ACC["HG02257|2"]=GCA_018466835.2
ACC["HG03516|1"]=GCA_018469425.2; ACC["HG03516|2"]=GCA_018469415.2

# Excluded: HG002 (hg002v1.1) uses a bespoke chr-name scheme that does not match
# its GenArk assembly's aliases (only pcLAI is present for it), and CHM13 (= hs1)
# has no annotation data here. Skip these samples.
declare -A SKIP=( [HG002|1]=1 [HG002|2]=1 [CHM13|0]=1 )

# build the job list (skip already-built outputs)
JOBS_FILE=$WORK/jobs.txt; : > "$JOBS_FILE"
for t in $TRACKS; do
  of=$(outfile "$t")
  tail -n +2 $IDX/idx_$t.csv | awk -F, '{print $1"\t"$2}' | sort -u \
  | while IFS=$'\t' read -r s h; do
      [ -n "${SKIP["$s|$h"]:-}" ] && continue      # excluded sample
      a=${ACC["$s|$h"]:-}
      [ -z "$a" ] && { echo "SKIP no-acc $t $s $h" >&2; continue; }
      # resume: skip only if the output is a VALID bigBed/bigWig (a killed run
      # can leave a non-empty but truncated file, which -s would accept)
      if [ "$FORCE" != 1 ] && valid "$HUB/$a/$of"; then continue; fi
      echo "$t $s $h $a" >> "$JOBS_FILE"
    done
done
echo "jobs to run: $(wc -l < "$JOBS_FILE")"

# run
# stats.tsv is APPENDED to, never truncated: the resume logic skips already-built
# outputs, so truncating would throw away the counts for every track this run did
# not touch. Each run is bracketed by a "# run <date>" comment line, and the
# summary below takes the LAST record for each (track,assembly).
RUNSTAMP=$(date +%Y-%m-%dT%H:%M:%S)
mkdir -p $WORK/log
printf '# run %s jobs=%s\n' "$RUNSTAMP" "$(wc -l < "$JOBS_FILE")" >> $WORK/log/stats.tsv
parallel --will-cite -j "$JOBS" --joblog $WORK/log/parallel.joblog --colsep ' ' \
  "$SCR/hprc2annotBuildOne.sh {1} {2} {3} {4}" :::: "$JOBS_FILE" \
  > $WORK/log/build.out 2> $WORK/log/build.err || true

echo "build finished; failures:"; grep -c FAIL $WORK/log/build.err 2>/dev/null || echo 0
echo "sequence names that did not match the assembly:"
grep -c UNMATCHED_SEQ $WORK/log/build.err 2>/dev/null || echo 0

# Per-track roll-up over the whole collection, from the last record per
# (track,assembly). This is the retained evidence for the feature-count claims
# in the makeDoc, so write it to a file rather than only to the terminal.
awk -F'\t' '!/^#/ { k=$1"\t"$2; in_[k]=$5; out[k]=$6; d[k]=$7; ur[k]=$8; un[k]=$9 }
  END { for (k in in_) { split(k,a,"\t"); t=a[1];
          n[t]++; ci[t]+=in_[k]; co[t]+=out[k]; cd[t]+=d[k]; cu[t]+=ur[k]; cn[t]+=un[k] }
        printf "track\tassemblies\tinput\toutput\tlost\tpastEnd\tunmatchedRows\tunmatchedNames\n"
        for (t in n) printf "%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
              t, n[t], ci[t], co[t], ci[t]-co[t], cd[t], cu[t], cn[t] }' \
  $WORK/log/stats.tsv | sort > $WORK/log/summary.tsv
echo "--- feature counts (also in log/summary.tsv) ---"; cat $WORK/log/summary.tsv

"$SCR/hprc2annotMakeTrackDb.py"
echo "DONE"
