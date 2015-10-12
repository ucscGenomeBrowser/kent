#!/bin/sh

export outside="/hive/data/outside/ncbi/genomes/refseq"
export inside="/hive/data/inside/ncbi/genomes/refseq"

# for T in archaea bacteria fungi invertebrate other plant protozoa vertebrate_mammalian vertebrate_other
for T in vertebrate_mammalian vertebrate_other
do
  nameFile="${outside}/latest.${T}.txt"
  cut -f2 "${nameFile}" | sed -e "s#^#${outside}/#; s#genomic.fna.gz#assembly_report.txt#" | while read F
  do
     if [ -s "${F}" ]; then
       ${inside}/scripts/idFromAsmRpt.pl "${F}"
     fi
  done
done
