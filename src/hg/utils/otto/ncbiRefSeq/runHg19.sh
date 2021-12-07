#!/bin/bash

set -beEu -o pipefail

export TOP="/hive/data/outside/otto/ncbiRefSeq"
cd "${TOP}"

export db="hg19"
export Db="Hg19"
export sumFile="prev${Db}.sum"
export asmId="GCF_000001405.25_GRCh37.p13"
export dateStamp=`date "+%F"`
export wrkDir="/hive/data/genomes/${db}/bed/ncbiRefSeq.p13.${dateStamp}"
export sumFile="${TOP}/prev${Db}.sum"
export gffFile="/hive/data/outside/ncbi/genomes/GCF/000/001/405/GCF_000001405.25_GRCh37.p13/GCF_000001405.25_GRCh37.p13_genomic.gff.gz"

${TOP}/runUcscDb.sh "${db}" "${Db}" "${sumFile}" "${asmId}" "${wrkDir}" "${gffFile}"
