#!/bin/bash
# Build the hprc2Chains composite: symlink the per-assembly bigChain files into
# gbdb and generate the trackDb .ra with one subtrack per haplotype (462).
# Subtracks are grouped by superpopulation (from the release sample metadata) and
# haplotype so the composite matrix can filter them.
# Redmine #35415
# Usage: hprc2Chains.sh <bedDir> <outRaPath>

set -beEu -o pipefail

BED="${1}"
outRa="${2}"
names="${BED}/naming/hprc2.names.tsv"        # sampleId  hap  assemblyName  shortLabel
meta="${BED}/naming/hprc_release2_sample_metadata.csv"

# sample metadata (population_abbreviation is column 4)
if [ ! -s "${meta}" ]; then
  wget -q -O "${meta}" \
    https://raw.githubusercontent.com/human-pangenomics/hprc_intermediate_assembly/main/data_tables/sample/hprc_release2_sample_metadata.csv
fi

# gbdb symlinks
mkdir -p /gbdb/hg38/hprc2/chains
while IFS=$'\t' read -r samp hap asm short; do
  ln -sf "${BED}/bigChains/${short}.bb"      /gbdb/hg38/hprc2/chains/${short}.bb
  ln -sf "${BED}/bigChains/${short}.link.bb" /gbdb/hg38/hprc2/chains/${short}.link.bb
done < "${names}"

# sample_id -> superpopulation (proper CSV parse; the metadata has quoted fields
# containing commas, so a naive comma split mis-assigns columns)
SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
declare -A SPOP
while IFS=$'\t' read -r sid spop; do
  SPOP["$sid"]="$spop"
done < <(python3 "${SCRIPTDIR}/hprc2SampleSuperpop.py" "${meta}")

# emit the composite
{
cat <<'HDR'
    track hprc2Chains
    parent hprc2
    compositeTrack on
    shortLabel HPRC2 Chains
    longLabel HPRC2 assembly chain alignments to GRCh38 (per haplotype)
    subGroup1 superpop Superpopulation AFR=African AMR=American EAS=East_Asian EUR=European SAS=South_Asian OTH=Other
    subGroup2 hap Haplotype h1=haplotype_1 h2=haplotype_2
    dimensions dimensionX=superpop dimensionY=hap
    sortOrder superpop=+ hap=+
    dragAndDrop subTracks
    type bigChain
    spectrum on
    visibility hide
    priority 4
    html hprc2

HDR

prio=0
while IFS=$'\t' read -r samp hap asm short; do
  prio=$((prio+1))
  spop="${SPOP[$samp]:-OTH}"
  sym=$(echo "$short" | tr -cd 'A-Za-z0-9')
  printf '        track chainHprc2%s\n' "$sym"
  printf '        parent hprc2Chains off\n'
  printf '        subGroups superpop=%s hap=h%s\n' "$spop" "$hap"
  printf '        shortLabel %s\n' "$short"
  printf '        longLabel %s hap%s (%s) chain alignment to GRCh38\n' "$samp" "$hap" "$asm"
  printf '        type bigChain\n'
  printf '        bigDataUrl /gbdb/hg38/hprc2/chains/%s.bb\n' "$short"
  printf '        linkDataUrl /gbdb/hg38/hprc2/chains/%s.link.bb\n' "$short"
  printf '        priority %s\n\n' "$prio"
done < "${names}"
} > "${outRa}"

echo "wrote ${outRa} with $(grep -c '^        track ' "${outRa}") chain subtracks"
