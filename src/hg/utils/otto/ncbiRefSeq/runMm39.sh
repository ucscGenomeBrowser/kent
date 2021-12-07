#!/bin/bash

set -beEu -o pipefail

export TOP="/hive/data/outside/otto/ncbiRefSeq"
cd "${TOP}"

export db="mm39"
export Db="Mm39"
export sumFile="prev${Db}.sum"
export asmId="GCF_000001635.27_GRCm39"
export dateStamp=`date "+%F"`
export wrkDir="/hive/data/genomes/${db}/bed/ncbiRefSeq.${dateStamp}"
export sumFile="${TOP}/prev${Db}.sum"
export gffFile="/hive/data/outside/ncbi/genomes/GCF/000/001/635/GCF_000001635.27_GRCm39/GCF_000001635.27_GRCm39_genomic.gff.gz"

${TOP}/runUcscDb.sh "${db}" "${Db}" "${sumFile}" "${asmId}" "${wrkDir}" "${gffFile}"
