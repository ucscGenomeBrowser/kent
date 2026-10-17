#!/bin/bash
# build the lrTransTabSap and lrTransTabSapExpr bigBeds from the downloaded files in ./orig
# run in /hive/data/genomes/hg38/bed/lrTrans/tabulasapiens
set -beEu -o pipefail
export LC_ALL=C
scriptDir=$(dirname $(readlink -f $0))
sizes=/hive/data/genomes/hg38/chrom.sizes

# sum the per-cell counts by tissue
$scriptDir/lrTransTabSapExpr.py orig/all_samples_pacbio_recollapsed_raw_counts_pbids_bc_anndata_preprocessed.h5ad tissueExpr.tsv

$scriptDir/lrTransTabSapToBed.py orig tissueExpr.tsv $sizes lrTransTabSap.tmp

sort -k1,1 -k2,2n lrTransTabSap.tmp.bed > lrTransTabSap.bed
sort -k1,1 -k2,2n lrTransTabSap.tmp.barChart.bed > lrTransTabSapExpr.bed
mv lrTransTabSap.tmp.categories lrTransTabSap.categories
rm -f lrTransTabSap.tmp.bed lrTransTabSap.tmp.barChart.bed

bedToBigBed -tab -type=bed12+24 -as=$scriptDir/lrTransTabSap.as -extraIndex=name,geneSymbol \
    lrTransTabSap.bed $sizes lrTransTabSap.bb
bedToBigBed -tab -type=bed6+5 -as=$scriptDir/../../../lib/barChartBed.as -extraIndex=name \
    lrTransTabSapExpr.bed $sizes lrTransTabSapExpr.bb
