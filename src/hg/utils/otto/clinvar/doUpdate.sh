#!/bin/bash
set -o errexit -o pipefail
export PATH=$PATH:/cluster/bin/x86_64/ # for bedSort and bedToBigBed
cd /hive/data/outside/otto/clinvar/
./clinVarToBed --auto --maxDiff 0.1
cp clinvarMain.hg19.bb /hive/data/genomes/hg19/bed/clinvar/clinvarMain.bb
cp clinvarCnv.hg19.bb /hive/data/genomes/hg19/bed/clinvar/clinvarCnv.bb
cp clinvarMain.hg38.bb /hive/data/genomes/hg38/bed/clinvar/clinvarMain.bb
cp clinvarCnv.hg38.bb /hive/data/genomes/hg38/bed/clinvar/clinvarCnv.bb
