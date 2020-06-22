#!/bin/bash

# downloads and builds the curated ClinGen Curated CNVS track
# assumes running in the build directory:
# /hive/data/outside/otto/clinGen/
set -beEu -o pipefail

WORKDIR=$1
mkdir -p ${WORKDIR}/clinGenCnv
cd ${WORKDIR}/clinGenCnv

echo "user anonymous otto@soe.ucsc.edu
cd /pub/dbVar/data/Homo_sapiens/by_study/gvf
ls nstd45*
bye" > ftp.isca.rsp

if [ -e release.list ]
then
    mv release.list prev.release.list
fi
touch prev.release.list
rm -f release.list

# connect and list a directory, result to file: ls.check
ftp -n -v -i ftp.ncbi.nlm.nih.gov 2>&1 < ftp.isca.rsp &> ls.check
grep "nstd45.*gvf.gz" ls.check | sort > release.list || echo "Error - no gvf files found"

# see if anything is changing, if so, notify, download, and build
diff prev.release.list release.list > release.diff || true
count=`wc -l release.diff | cut -d' ' -f1`
if [ "${count}" -gt 1 ]
then
    echo "New ClinGen CNV update"
    today=`date +%F`
    mkdir -p ${today}/{download,output}
    cd ${today}/download
    hgsql -Ne 'select 0, ca.alias, size, ca.chrom, size from chromInfo ci join chromAlias ca on ci.chrom = ca.chrom where source = "refseq"' hg19 > hg19.lift
    hgsql -Ne 'select 0, ca.alias, size, ca.chrom, size from chromInfo ci join chromAlias ca on ci.chrom = ca.chrom where source = "refseq"' hg38 > hg38.lift
    for grc in GRCh37 GRCh38
    do
        wget -N -q "ftp://ftp.ncbi.nlm.nih.gov/pub/dbVar/data/Homo_sapiens/by_study/gvf/nstd45.${grc}.variant*.gvf.gz"
        if [ ${grc} == "GRCh37" ]
        then
            zcat nstd45.${grc}.* | ../../../processClinGenCnv.py stdin hg19.lift | sort -k1,1 -k2,2n | bedClip -truncate stdin /hive/data/genomes/hg19/chrom.sizes stdout > ../output/hg19.clinGenCnv.bed
            bedToBigBed -type=bed9+17 -as=../../clinGenCnv.as -tab ../output/hg19.clinGenCnv.bed /hive/data/genomes/hg19/chrom.sizes ../output/hg19.clinGenCnv.bb
            cp ../output/hg19.clinGenCnv.bb ${WORKDIR}/release/hg19/clinGenCnv.bb
        elif [ ${grc} == "GRCh38" ]
        then
            zcat nstd45.${grc}.* | ../../../processClinGenCnv.py stdin hg38.lift | sort -k1,1 -k2,2n | bedClip -truncate stdin /hive/data/genomes/hg38/chrom.sizes stdout  > ../output/hg38.clinGenCnv.bed
            bedToBigBed -type=bed9+17 -as=../../clinGenCnv.as -tab ../output/hg38.clinGenCnv.bed /hive/data/genomes/hg38/chrom.sizes ../output/hg38.clinGenCnv.bb
            cp ../output/hg38.clinGenCnv.bb ${WORKDIR}/release/hg38/clinGenCnv.bb
        fi
    done
    cd ..
else
    echo "No ClinGen CNV update"
fi
