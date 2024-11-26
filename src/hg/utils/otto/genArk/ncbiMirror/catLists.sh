#!/bin/bash

if [ $# -ne 1 ]; then
  printf "usage: catLists.sh [genbank|refseq]\n" 1>&2
  exit 255
fi

export type=$1

export TOP="/hive/data/outside/ncbi/genomes/reports/$type"
export hist="${TOP}/history"
export YYYY=`date "+%Y"`
export MM=`date "+%m"`
export DS=`date "+%F"`
export logDir="${TOP}/history/$YYYY/$MM"

if [ ! -d "${logDir}" ]; then
  mkdir -p "${logDir}"
fi

# echo $YYYY $MM $DS $logDir

awk -F$'\t' 'length($5) > 0' allAssemblies.taxonomy.tsv | grep ";Primates;" \
   | cut -f2-4 | sort -u > ${logDir}/primates.${DS}.list

awk -F$'\t' 'length($5) > 0' allAssemblies.taxonomy.tsv | grep ";Mammalia;" | grep -v ";Primates;" \
   | cut -f2-4 | sort -u > ${logDir}/mammal.${DS}.list

awk -F$'\t' 'length($5) > 0' allAssemblies.taxonomy.tsv | grep ";Aves;" \
   | cut -f2-4 | sort -u > ${logDir}/birds.${DS}.list

awk -F$'\t' 'length($5) > 0' allAssemblies.taxonomy.tsv | grep ";Actinopterygii;" \
   | cut -f2-4 | sort -u > ${logDir}/fish.${DS}.list

awk -F$'\t' 'length($5) > 0' allAssemblies.taxonomy.tsv | grep ";Vertebrata;" \
   | egrep -v ";Primates;|;Mammalia;|;Aves;|;Actinopterygii;" | cut -f2-4 | sort -u > ${logDir}/vertebrate.${DS}.list

export vertCount=`awk -F$'\t' 'length($5) > 0' allAssemblies.taxonomy.tsv | grep ";Vertebrata;" | cut -f2-4 | sort -u | wc -l`

printf "# this is catLists.sh $type, all vertebrates count: $vertCount\n" 1>&2

for T in primates mammal birds fish vertebrate
do
  if [ -s "${hist}/prev.${T}.list" ]; then
     printf "# catLists.sh $type comparing prev.$T.list to $T.$DS.list\n" 1>&2
     ls -ogL history/prev.${T}.list history/$YYYY/$MM/$T.$DS.list 2> /dev/null | sed -e 's/^/# /;' 1>&2
     export lc=`diff "${hist}/prev.${T}.list" ${logDir}/${T}.${DS}.list \
	| egrep -v "^[0-9]" | wc -l`
     if [ "${lc}" -gt 0 ]; then
       printf "# ${T} (< previous, > newest)\n"
     diff "${hist}/prev.${T}.list" ${logDir}/${T}.${DS}.list | egrep -v "^[0-9]"
     fi
     rm "${hist}/prev.${T}.list"
     ln -s "${logDir}/${T}.${DS}.list" "${hist}/prev.${T}.list"
  fi
done
# 27679	GCF_000235385.1	SaiBol1.0	Saimiri boliviensis boliviensis	cellular organisms;Eukaryota;Opisthokonta;Metazoa;Eumetazoa;Bilateria;Deuterostomia;Chordata;Craniata;Vertebrata;Gnathostomata;Teleostomi;Euteleostomi;Sarcopterygii;Dipnotetrapodomorpha;Tetrapoda;Amniota;Mammalia;Theria;Eutheria;Boreoeutheria;Euarchontoglires;Primates;Haplorrhini;Simiiformes;Platyrrhini;Cebidae;Saimiriinae;Saimiri;


# 9031	GCF_000002315.4	Gallus_gallus-5.0	Gallus gallus	cellular organisms;Eukaryota;Opisthokonta;Metazoa;Eumetazoa;Bilateria;Deuterostomia;Chordata;Craniata;Vertebrata;Gnathostomata;Teleostomi;Euteleostomi;Sarcopterygii;Dipnotetrapodomorpha;Tetrapoda;Amniota;Sauropsida;Sauria;Archelosauria;Archosauria;Dinosauria;Saurischia;Theropoda;Coelurosauria;Aves;Neognathae;Galloanserae;Galliformes;Phasianidae;Phasianinae;Gallus;
