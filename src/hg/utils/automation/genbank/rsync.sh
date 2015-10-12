#!/bin/sh

cd /hive/data/outside/ncbi/genomes/genbank

rsync -avPL \
  rsync://ftp.ncbi.nlm.nih.gov/genomes/genbank/assembly_summary_genbank.txt ./
rsync -avPL \
  rsync://ftp.ncbi.nlm.nih.gov/genomes/genbank/README.txt ./

for D in vertebrate_mammalian vertebrate_other archaea bacteria fungi invertebrate other plant protozoa
do
  mkdir -p ${D}
  rsync -avPL --include "*/" --include "*_assembly_report.txt" --exclude "*" \
    rsync://ftp.ncbi.nlm.nih.gov/genomes/genbank/${D}/ ./${D}/
  rsync -avPL --include "*/" --include="chr2acc" --include="*.chr2scaf" \
     --include "*_rm.out.gz" --include "*.agp.gz" \
       --include "*_assembly_report.txt" --exclude "*" \
          rsync://ftp.ncbi.nlm.nih.gov/genomes/genbank/${D}/ ./${D}/
done
