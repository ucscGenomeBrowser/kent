#!/bin/bash
# Aggregate the per-assembly HPRC2 intermediates into the final hg38 native tracks.
# Redmine #35415
# Usage: hprc2Aggregate.sh <section> <bedDir>
#   section = coverage | breaks | arrange | all
#
# Colors (match lrSv svColor palette): del=200,0,0  ins=0,0,200  double(CPX)=140,0,200
#                           inv=230,140,0  dup=0,160,0

set -beEu -o pipefail
export PATH=/cluster/bin/x86_64:/cluster/bin/scripts:$HOME/bin/x86_64:$PATH
ulimit -s unlimited || true
export TMPDIR="${2}/sortTmp"; mkdir -p "$TMPDIR"

section="${1}"
BED="${2}"
hgSizes=/hive/data/genomes/hg38/chrom.sizes
AS=$HOME/kent/src/hg/makeDb/scripts/hprc2/hprc2Arrange.as
N=$(ls "${BED}"/chain/*.chain | wc -l)          # number of assemblies actually built
out="${BED}/out"; mkdir -p "${out}"
echo "aggregating section=${section} over N=${N} assemblies"

do_coverage() {
  cd "${BED}"
  mkdir -p singleCover
  ls bigChains/*.link.bb | parallel -j 16 '
     n=$(basename {} .link.bb);
     bigBedToBed {} stdout | bedSingleCover.pl stdin > '"${BED}"'/singleCover/$n.bed'
  gnusort -S32G --parallel=16 -k1,1 -k2,2n singleCover/*.bed > allLinks.bed
  bedItemOverlapCount hg38 allLinks.bed > allLinks.bedGraph
  awk -v n="${N}" '{printf "%s\t%s\t%s\t%.4f\n",$1,$2,$3,$4/n}' allLinks.bedGraph \
     > cov.norm.bedGraph
  bedInvert.pl "${hgSizes}" cov.norm.bedGraph > cov.zero.bed
  ( awk '{printf "%s\t%s\t%s\t0\n",$1,$2,$3}' cov.zero.bed; cat cov.norm.bedGraph ) \
     | gnusort -S32G --parallel=16 -k1,1 -k2,2n > cov.full.bedGraph
  bedGraphToBigWig cov.full.bedGraph "${hgSizes}" "${out}/hprc2Coverage.bw"
  echo "coverage done: $(ls -la ${out}/hprc2Coverage.bw | awk '{print $5}') bytes"
}

do_breaks() {
  cd "${BED}"
  mkdir -p breaks
  ls bigChains/*.bb | grep -v '.link.bb' | parallel -j 16 '
     n=$(basename {} .bb);
     bigChainBreaks {} $n '"${BED}"'/breaks/$n.txt 2>/dev/null'
  # aggregate: same break position across assemblies -> count + source list; color by prevalence
  thr=$(( N / 4 ))                                  # "common" break = seen in >25% of assemblies
  sort -k1,1 -k2,2n breaks/*.txt | uniq \
   | awk -v thr="${thr}" 'BEGIN{OFS="\t"}
      {
        if(($1==l1)&&($2==l2)){sc++; names=names","$3}
        else{
          if(NR>1) print l1,l2,l2,"brk"c,(sc>1000?1000:sc),"+",l2,l2,(sc>thr?"200,0,0":"230,140,0"),names
          sc=1; names=$3; c++; l1=$1; l2=$2
        }
      }
      END{ if(l1!="") print l1,l2,l2,"brk"c,(sc>1000?1000:sc),"+",l2,l2,(sc>thr?"200,0,0":"230,140,0"),names }' \
   | sort -k1,1 -k2,2n > breaks.bed9
  bedToBigBed -tab -type=bed9+1 -as=$HOME/kent/src/hg/lib/chainBreaks.as \
     breaks.bed9 "${hgSizes}" "${out}/hprc2Breaks.bb"
  echo "breaks done: $(wc -l < breaks.bed9) rows"
}

do_arrange() {
  cd "${BED}"
  # ---- indels: combine, sort once, split by type ----
  # (resume guard: the sort of ~377M rows is the slow step)
  if [ ! -s sortIndels.txt ]; then
    gnusort -S32G --parallel=16 -k1,1 -k2,2n -k3,3n -k5,5n arr/*.indel.txt > sortIndels.txt
  fi

  # deletions: end>start & col5==0 ; size = end-start
  awk '($3>$2)&&($5==0)' sortIndels.txt \
   | chainArrangeCollect -exact arrDel stdin stdout \
   | awk 'BEGIN{OFS="\t"}{s=$3-$2;
        print $1,$2,$3,$4,($5>1000?1000:$5),$6,$7,$8,"200,0,0",$10,0,$5":"s"bp",s,
              "Deletion relative to GRCh38<br>Size: "s" bp<br>Assemblies with deletion: "$5}' \
   | gnusort -S16G -k1,1 -k2,2n > del.bed
  # insertions: end==start ; size = querySize (col11)
  awk '$3==$2' sortIndels.txt \
   | chainArrangeCollect -exact arrIns stdin stdout \
   | awk 'BEGIN{OFS="\t"}{s=$11;
        print $1,$2,$3,$4,($5>1000?1000:$5),$6,$7,$8,"0,0,200",$10,$11,$5":"s"bp",s,
              "Insertion relative to GRCh38<br>Size: "s" bp<br>Assemblies with insertion: "$5}' \
   | gnusort -S16G -k1,1 -k2,2n > ins.bed
  # double (both sides): end>start & col5!=0 ; size = max(del,ins)
  awk '($3>$2)&&($5!=0)' sortIndels.txt \
   | chainArrangeCollect -exact arrDbl stdin stdout \
   | awk 'BEGIN{OFS="\t"}{d=$3-$2;i=$11;s=(d>i?d:i);
        print $1,$2,$3,$4,($5>1000?1000:$5),$6,$7,$8,"140,0,200",$10,$11,$5":"i"i/"d"d bp",s,
              "Complex indel relative to GRCh38<br>Inserted: "i" bp<br>Deleted: "d" bp<br>Assemblies: "$5}' \
   | gnusort -S16G -k1,1 -k2,2n > double.bed

  # ---- rearrangements: inversions + duplications ----
  gnusort -S24G --parallel=16 -k1,1 -k2,2n -k3,3n -k5,5n arr/*.inv.txt \
   | chainArrangeCollect arrInv stdin stdout \
   | awk 'BEGIN{OFS="\t"}{s=$3-$2;
        print $1,$2,$3,$4,($5>1000?1000:$5),$6,$7,$8,"230,140,0",$10,$11,$5":"s"bp",s,
              "Inversion relative to GRCh38<br>Size: "s" bp<br>Assemblies with inversion: "$5}' \
   | gnusort -S16G -k1,1 -k2,2n > inv.bed
  gnusort -S24G --parallel=16 -k1,1 -k2,2n -k3,3n -k5,5n arr/*.dup.txt \
   | chainArrangeCollect arrDup stdin stdout \
   | awk 'BEGIN{OFS="\t"}{s=$3-$2;
        print $1,$2,$3,$4,($5>1000?1000:$5),$6,$7,$8,"0,160,0",$10,$11,$5":"s"bp",s,
              "Duplication relative to GRCh38<br>Size: "s" bp<br>Assemblies with duplication: "$5}' \
   | gnusort -S16G -k1,1 -k2,2n > dup.bed

  # ---- build bigBeds; report max size for filterLimits ----
  for t in del ins double inv dup; do
    bedToBigBed -tab -type=bed9+5 -as="${AS}" "${t}.bed" "${hgSizes}" "${out}/hprc2${t^}.bb"
    mx=$(awk 'BEGIN{m=0}$13>m{m=$13}END{print m}' "${t}.bed")
    printf "  %-7s rows=%s maxSize=%s -> %s\n" "$t" "$(wc -l < ${t}.bed)" "$mx" "${out}/hprc2${t^}.bb"
  done
}

case "${section}" in
  coverage) do_coverage ;;
  breaks)   do_breaks ;;
  arrange)  do_arrange ;;
  all)      do_coverage; do_breaks; do_arrange ;;
  *) echo "unknown section: ${section}"; exit 1 ;;
esac
echo "aggregate ${section} complete"
