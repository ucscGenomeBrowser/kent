#!/bin/bash

set -beEu -o pipefail

export TOP="/hive/data/outside/otto/ncbiRefSeq"
cd "${TOP}"

export db="mm10"
export Db="Mm10"
export sumFile="prev${Db}.sum"
export asmId="GCF_000001635.26_GRCm38.p6"
export dateStamp=`date "+%F"`
export wrkDir="/hive/data/genomes/${db}/bed/ncbiRefSeq.p6.${dateStamp}"
export sumFile="${TOP}/prev${Db}.sum"
export gffFile="/hive/data/outside/ncbi/genomes/GCF/000/001/635/GCF_000001635.26_GRCm38.p6/GCF_000001635.26_GRCm38.p6_genomic.gff.gz"

${TOP}/runUcscDb.sh "${db}" "${Db}" "${sumFile}" "${asmId}" "${wrkDir}" "${gffFile}"
