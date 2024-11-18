#!/bin/bash

if [ $# -ne 1 ]; then
  printf "usage: updateLists.sh [genbank|refseq]\n" 1>&2
  exit 255
fi

# something here needs this full shell environment
# otherwise the taxId listings do not get the full taxonomy string

. $HOME/.bashrc.hiram

export type=$1

cd /hive/data/outside/ncbi/genomes/reports/$1

export TOP="/hive/data/outside/ncbi/genomes/reports/$1"
export hist="${TOP}/history"
export YYYY=`date "+%Y"`
export MM=`date "+%m"`
export DS=`date "+%F"`
export logDir="${TOP}/history/$YYYY/$MM"
export updateLog="${logDir}/updateLog.${DS}"

if [ ! -d "${logDir}" ]; then
  mkdir -p "${logDir}"
fi

if [ ! -s ${logDir}/primates.wiki.txt.${DS} ]; then
  cp -p primates.wiki.txt ${logDir}/primates.wiki.txt.${DS}
fi

if [ ! -s ${logDir}/allAssemblies.taxonomy.tsv.${DS} ]; then
  cp -p allAssemblies.taxonomy.tsv ${logDir}/allAssemblies.taxonomy.tsv.${DS}
  cp -p allAssemblies.commonNames.tsv ${logDir}/allAssemblies.commonNames.tsv.${DS}
fi

export count0=`cat allAssemblies.taxonomy.tsv | wc -l`
printf "# updating allAssemblies.taxonomy.tsv, line count before: %d\n" "$count0" 1>&2
./taxonomyNames.pl ../assembly_summary_$1.txt 2>> "${updateLog}"
./taxonomyNames.pl ../assembly_summary_${1}_historical.txt 2>> "${updateLog}"
export count1=`cat allAssemblies.taxonomy.tsv | wc -l`
printf "# updating allAssemblies.taxonomy.tsv,  line count after: %d\n" "$count1" 1>&2

export count2=`cat allAssemblies.commonNames.tsv | wc -l`
printf "# updating allAssemblies.commonNames.tsv, line count before: %d\n" "$count2" 1>&2
./commonNames.pl ../assembly_summary_$1.txt 2>> "${updateLog}"
./commonNames.pl ../assembly_summary_${1}_historical.txt 2>> "${updateLog}"
export count3=`cat allAssemblies.commonNames.tsv | wc -l`
printf "# updating allAssemblies.commonNames.tsv, line count after: %d\n" "$count3" 1>&2

./allVerts.sh > $logDir/$DS.vertData 2>> "${updateLog}"

printf "# allAssemblies.taxonomy.tsv went from %d to %d counts\n" "$count0" "$count1"
printf "# allAssemblies.commonNames.tsv went from %d to %d counts\n" "$count2" "$count3"

./primates.pl -wiki > primates.wiki.txt 2>> "${updateLog}"
