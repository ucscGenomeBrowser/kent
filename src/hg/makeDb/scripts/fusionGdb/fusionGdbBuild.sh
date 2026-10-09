#!/bin/bash
# Build the FusionGDB bigBed and search index for hg19 and hg38.
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
    python3 $scriptDir/fusionGdbToBed.py $info $orf $db fusionGdb.bed fusionGdb.ix.txt
    bedToBigBed -tab -type=bed9+16 -as=$scriptDir/fusionGdb.as -extraIndex=name \
        fusionGdb.bed /hive/data/genomes/$db/chrom.sizes _fusionGdb.bb
    ixIxx fusionGdb.ix.txt _fusionGdb.ix _fusionGdb.ixx
    # the same events as kept transcript parts, bigGenePred
    python3 $scriptDir/fusionGdbToBigGenePred.py $info $orf $dl $db fusionGdbTx.bed
    bedToBigBed -tab -type=bed12+23 -as=$scriptDir/fusionGdbTx.as -extraIndex=name \
        fusionGdbTx.bed /hive/data/genomes/$db/chrom.sizes _fusionGdbTx.bb
    # decorator: a bar glyph on the breakpoint end of each transcript part
    python3 $scriptDir/fusionGdbTxDecorator.py fusionGdbTx.bed fusionGdbTxDecorator.bed
    bedToBigBed -tab -type=bed12+4 -as=$scriptDir/../../../lib/decoration.as \
        fusionGdbTxDecorator.bed /hive/data/genomes/$db/chrom.sizes _fusionGdbTxDecorator.bb
done
