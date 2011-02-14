Instructions for running GEO packager for ENCODE data

-----------
TODO:  Add tests with dummy input data:

1) dummy metaDb's, imported from files checked in to input/ dir.
2) dummy expVar.ra
3) dummy files in fake compositeDir;
* only needs to test -soft option and diff with expected/
------------

1) Add an entry for the composite to config/expVars.ra
2) Create .soft file to test it, e.g.:

encodeMkGeoPkg hg18 wgEncodeCaltechRnaSeq -soft <dummy_seq_platform>

Creates file:  ucsc_encode_dcc_wgEncodeCaltechRnaSeq.soft

Command line options for database and release have also been added.
The command would take the form:

encodeMkGeoPkg hg18 wgEncodeCaltechRnaSeq -soft <dummy_seq_platform>\
	-database localhost -release release1

Default values for these options are "mysqlbeta" for database and
the downloads file for release.

3) To complete submission, run w/o -soft option:

encodeMkGeoPkg hg18 wgEncodeCaltechRnaSeq <dummy_seq_platform>

This creates a directory: ucsc_encode_dcc_wgEncodeCaltechRnaSeq
containing all files in downloads directory that are referenced
by metaDb for that composite.

4) Hand-craft .soft file:

  a) Search and replace <dummy_seq_platform> with real sequencing platform,
  b) Title (use long label for example),
  c) Summary text,
  d) Overall design, and
  e) Contributors (pulled from Credits section of track description).

5) Check in .soft time to ~/kent/src/hg/encode/geoSofts/.

6) bzip <submission dir> (lengthy).

7) Use aspera to send bzipped dir and .soft to GEO:

 /opt/aspera/connect/bin/ascp -i ~/encode_geo_key/encode_geo_key.ppk -QTr -l300m \
    ucsc_encode_dcc_wgEncodeCaltechRnaSeq.tar.bz2 \
    ucsc_encode_dcc_wgEncodeCaltechRnaSeq.soft \
    asp-geo@upload.ncbi.nlm.nih.gov:ENCODE/

8) Mail GEO (Pierre Ledoux, ledoux@ncbi.nlm.nih.gov) to notify that files have
been uploaded (say which ones).

7) Verify GSE and GSM entries are correct, and that the GSE appears on the GEO ENCODE
page:  http://www.ncbi.nlm.nih.gov/geo/info/ENCODE.html

8) Load accession numbers into metaDb table

encodeAddGeoAccessions hg18 \
    ~/kent/src/hg/encode/geoSofts/geoSofts/ucsc_encode_dcc_wgEncodeCaltechRnaSeq.soft \
    caltech_geo_table.txt >mdbupdate_cmds

Run mdbupdate_cmds to update mdb and then do standard procedure to commit mdb
changes.

Example output of encodeMkGeoPkg:
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


