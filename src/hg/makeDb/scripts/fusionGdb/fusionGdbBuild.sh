#!/bin/bash
# Build the FusionGDB bigGenePred, its decorator and search index for hg19 and hg38.
# Run from /hive/data/genomes/hg38/bed/fusionGdb after fusionGdbDownload.sh
# has filled the download/ subdirectory.
# The FusionGDB terms do not allow redistribution. File names starting with "_" are
# excluded from the rsync of /gbdb to hgdownload, so the output files start with "_".
set -beEu -o pipefail

scriptDir=$(dirname $(readlink -f $0))
dl=/hive/data/genomes/hg38/bed/fusionGdb/download
info=$dl/TCGA_ChiTaRS_combined_fusion_information_on_hg19.txt
orf=$dl/TCGA_ChiTaRS_combined_fusion_ORF_analyzed_gencode_h19v19.txt

for db in hg19 hg38; do
    out=/hive/data/genomes/$db/bed/fusionGdb
    mkdir -p $out
    cd $out
    # fusion events as the kept transcript parts of both partners, bigGenePred
    python3 $scriptDir/fusionGdbToBigGenePred.py $info $orf $dl $db fusionGdbTx.bed
    bedToBigBed -tab -type=bed12+24 -as=$scriptDir/fusionGdbTx.as -extraIndex=name \
        fusionGdbTx.bed /hive/data/genomes/$db/chrom.sizes _fusionGdbTx.bb
    # search index: feature ID -> fusion name as A::B, A-B and A_B (geneName is field 18)
    awk -F'\t' '{split($18, g, "::"); print $4, $18, g[1]"-"g[2], g[1]"_"g[2]}' fusionGdbTx.bed \
        > fusionGdbTx.ix.txt
    ixIxx fusionGdbTx.ix.txt _fusionGdbTx.ix _fusionGdbTx.ixx
    # decorator: an arrow glyph on the breakpoint end of each transcript part
    python3 $scriptDir/fusionGdbTxDecorator.py fusionGdbTx.bed fusionGdbTxDecorator.bed
    bedToBigBed -tab -type=bed12+5 -as=$scriptDir/fusionGdbTxDecorator.as \
        fusionGdbTxDecorator.bed /hive/data/genomes/$db/chrom.sizes _fusionGdbTxDecorator.bb
done
