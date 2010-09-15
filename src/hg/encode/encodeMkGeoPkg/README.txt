Instructions for running GEO packager for ENCODE data

1) Add an entry for the composite to config/expVars.ra
2) Create .soft file, e.g.:

encodeMkGeoPkg hg18 wgEncodeCaltechRnaSeq -soft dummy_seq_platform

Creates file:  ucsc_encode_dcc_wgEncodeCaltechRnaSeq.soft

Example output:
--------------------------------------

Number of objects in composite track: 286
Number of experimental variables: 5
        cell
        localization
        readType
        insertLength
        replicate
WARNING: no raw files found for Myers_GM12878_cell_1x32_ilNA_2
WARNING: file /hive/groups/encode/dcc/analysis/ftp/pipeline/hg18/wgEncodeCaltechRnaSeq/wgEncodeCaltechRnaSeqFastqRd2Rep5H1hescCellPapNaR2x75.fastq.gz not found
WARNING: file /hive/groups/encode/dcc/analysis/ftp/pipeline/hg18/wgEncodeCaltechRnaSeq/wgEncodeCaltechRnaSeqFastqRd1Rep5H1hescCellPapNaR2x75.fastq.gz not found
WARNING: file /hive/groups/encode/dcc/analysis/ftp/pipeline/hg18/wgEncodeCaltechRnaSeq/wgEncodeCaltechRnaSeqRawData6Rep5H1hescCellPapErng32aR2x75.rpkm.gz not found
WARNING: file /hive/groups/encode/dcc/analysis/ftp/pipeline/hg18/wgEncodeCaltechRnaSeq/wgEncodeCaltechRnaSeqRawData5Rep5H1hescCellPapErng32aR2x75.rpkm.gz not found
WARNING: file /hive/groups/encode/dcc/analysis/ftp/pipeline/hg18/wgEncodeCaltechRnaSeq/wgEncodeCaltechRnaSeqRawSignalRep5H1hescCellPapBb2R2x75.wig.gz not found
WARNING: file /hive/groups/encode/dcc/analysis/ftp/pipeline/hg18/wgEncodeCaltechRnaSeq/wgEncodeCaltechRnaSeqPairedRep5H1hescCellPapErng32aR2x75.bed12.gz not found
WARNING: file /hive/groups/encode/dcc/analysis/ftp/pipeline/hg18/wgEncodeCaltechRnaSeq/wgEncodeCaltechRnaSeqSplicesRep5H1hescCellPapBb2R2x75.bed12.gz not found
WARNING: no files found for Myers_H1-hESC_cell_2x75_200_5
WARNING: no raw files found for Myers_K562_cell_1x32_ilNA_2
WARNING: no raw files found for Myers_GM12878_cell_1x32_ilNA_1
WARNING: no raw files found for Myers_K562_cell_1x32_ilNA_1


