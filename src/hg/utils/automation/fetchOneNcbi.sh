#!/bin/bash

# exit on any error
set -beEu -o pipefail

if [ $# -ne 1 ]; then
  echo "usage: ./fetchOneNcbi.sh <db>" 1>&2
  echo "where <db> is a new UCSC database name" 1>&2
  echo "   and /hive/data/genomes/<db>/genbank does not yet exist" 1>&2
  echo "expects HOME/kent/src/hg/utils/phyloTrees/db.ncbiTaxId.tab" 1>&2
  echo "to be up to date." 1>&2
  echo "result is a copy of files from:" 1>&2
  echo "    ftp.ncbi.nlm.nih.gov/genbank/genomes/Eukaryotes/" 1>&2
  echo "into /hive/data/genomes/<db>/genbank/" 1>&2
  exit 255
fi

export db="$1"
grep `echo $db | sed -e 's/[0-9][0-9]*//'` $HOME/kent/src/hg/utils/phyloTrees/db.ncbiTaxId.tab  | sort | cut -f5 | tail -1 | sed -e 's#/ASSEMB.*##' | while read ftpDir
do
  export destDir="/hive/data/genomes/${db}/genbank"
  export srcDir="ftp.ncbi.nlm.nih.gov/genbank/genomes/Eukaryotes/${ftpDir}"

  if [ ! -d "${destDir}" ]; then
    mkdir -p "${destDir}"
  else
    echo "ERROR: <db> directory already exists:" 1>&2
    echo "   ${destDir}" 1>&2
    exit 255
  fi

  cd "${destDir}"
  rsync -a -P rsync://${srcDir}/ ./
done
