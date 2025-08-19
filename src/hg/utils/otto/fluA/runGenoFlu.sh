#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

if [[ $# > 0 ]]; then
    today=$1
else
    today=$(date +%F)
fi

fluADir=/hive/data/outside/otto/fluA
fluANcbiDir=$fluADir/ncbi/ncbi.latest
threads=16

cd $fluADir/genoflu

# Map GenBank accessions to full name for HA clade 2.3.3.4b sequences.
asmAcc=GCF_000864105.1
segRef=NC_007362.1
tawk '$2 == "2.3.4.4b" {print $1;}' $fluADir/build/$today/nextclade.$asmAcc.$segRef.tsv \
| grep -Fwf - <(cut -f 2 $fluADir/build/$today/renaming.tsv) \
| cut -d\| -f 1 \
| grep -Fwf - <(grep -vF '|EPI' $fluADir/build/$today/renaming.tsv | grep -vF '|SRR') \
| sort \
      > accToName.2.3.4.4b.tsv

# Now join in segment info from metadata to those
tawk '$17 != "" { print $1, $17; }' $fluANcbiDir/metadata.tsv \
| grep -Fwf <(cut -f 1 accToName.2.3.4.4b.tsv) \
| sort \
| join -t$'\t' accToName.2.3.4.4b.tsv - \
       > accToNameSeg.2.3.4.4b.tsv

# Rename sequences for genoflu-multi which expects the same name for all 8 segments.
# The filename has to end in .fasta not .fa!
xzcat $fluANcbiDir/genbank.fa.xz \
| faSomeRecords stdin <(cut -f 1 accToNameSeg.2.3.4.4b.tsv) stdout \
| faRenameRecords stdin <(cut -d\| -f 1 accToNameSeg.2.3.4.4b.tsv) genofluIn.fasta

# Add all of the Andersen lab sequences (with segment removed from name) to genofluIn.fasta
fastaNames $fluADir/andersen_lab.srrNotGb.renamed.fa \
| perl -wne 'chomp; $noSeg = $_; $noSeg =~ s@_(PB2|PB1|PA|HA|NP|NA|MP|NS)/@/@; print "$_\t$noSeg\n";' \
       > segNameToNoSegName.tsv
faRenameRecords $fluADir/andersen_lab.srrNotGb.renamed.fa segNameToNoSegName.tsv stdout \
                >> genofluIn.fasta
conda activate genoflu
# Run genoflu-multi in its source directory
pushd ~/github/GenoFLU-multi
time python bin/genoflu-multi.py -f $fluADir/genoflu
popd
conda deactivate

# Make a mapping of sample name and segment to full name for genofluToSegmentMetadata.py
perl -wne 'chomp; @w=split(/\t/); @c=split(/\|/, $w[1]); print "$c[0]\t$w[2]\t$w[1]\n";' \
     accToNameSeg.2.3.4.4b.tsv \
| sort \
     > sampleSetToName.2.3.4.4b.tsv
cut -f 1 segNameToNoSegName.tsv \
| perl -wne 'chomp; die if (! m@(.*)_(PB2|PB1|PA|HA|NP|NA|MP|NS)(/.*)@); print "$1$3\t$2\t$_\n";' \
    >> sampleSetToName.2.3.4.4b.tsv

$fluAScriptDir/genofluToSegmentMetadata.py results/results.tsv sampleSetToName.2.3.4.4b.tsv genoflu.tsv
wc -l genoflu.tsv
