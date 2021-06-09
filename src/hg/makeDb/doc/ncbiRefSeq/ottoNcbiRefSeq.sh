#!/bin/bash

set -beEu -o pipefail

export TOP="/hive/data/outside/ncbi/genomes/ncbiRefSeq"
cd "${TOP}"

./runHg38.sh
./runMm39.sh
./runHg19.sh
./runMm10.sh
