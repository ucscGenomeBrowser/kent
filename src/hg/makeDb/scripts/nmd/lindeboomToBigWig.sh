#!/bin/bash
# Convert Lindeboom et al. NMDetective .ct (bedGraph custom track) files to bigWig
# These are NMD efficiency predictions from Lindeboom et al. 2019, Nat Genet.

set -beEu -o pipefail

db=hg38
inDir=/hive/data/genomes/$db/bed/nmd/lindeboom
gbdbDir=/gbdb/$db/nmd
chromSizes=/hive/data/genomes/$db/chrom.sizes

mkdir -p $gbdbDir

for ct in $inDir/*.ct; do
    base=$(basename $ct .ct)
    echo "Converting $base"
    # strip the track header line then sort and convert
    tail -n +2 $ct | sort -k1,1 -k2,2n > $inDir/$base.bedGraph
    bedGraphToBigWig $inDir/$base.bedGraph $chromSizes $inDir/$base.bw
    rm $inDir/$base.bedGraph
    ln -sf $inDir/$base.bw $gbdbDir/$base.bw
done

echo "Done. bigWig files in $inDir, symlinked from $gbdbDir:"
ls -l $gbdbDir/*.bw
