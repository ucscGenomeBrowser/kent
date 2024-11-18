#!/bin/bash

cd /hive/data/outside/ncbi/genomes/reports/newAsm

grep -v "^#" ../assembly_summary_refseq.txt \
   | awk -F$'\t' '{printf "%d\t%s_%s\n", $6,$1,$16}' \
      | egrep -v "GCF_016700215.2_bGalGal1.pat.whiteleghornlayer.GRCg7w_WZ|Guppy_female_1.0\+MT|GCF_000842245.1_ViralProj14284" \
      | sed -e 's/ /_/g;' | sort > taxId.asmId.refseq.list

grep -v "^#" ../assembly_summary_refseq.txt \
   | egrep "reference genome|representative genome" \
     | awk -F$'\t' '{printf "%d\t%s_%s\n", $6,$1,$16}' \
       | egrep -v "GCF_016700215.2_bGalGal1.pat.whiteleghornlayer.GRCg7w_WZ|Guppy_female_1.0\+MT|GCF_000842245.1_ViralProj14284" \
        | sed -e 's/ /_/g;' | sort > taxId.asmId.refseq.reference.list

grep -w suppressed ../assembly_summary_refseq_historical.txt | cut -f1,16 \
    | sed -e 's/[ 	]/_/g;' | sort -u > refseq.suppressed.asmId.list

join -v 1 -1 2 <(sort -k2,2  taxId.asmId.refseq.list) \
   refseq.suppressed.asmId.list | awk '{printf "%d\t%s\n", $2, $1}' \
     | egrep -v "GCF_016700215.2_bGalGal1.pat.whiteleghornlayer.GRCg7w_WZ|Guppy_female_1.0\+MT|GCF_000842245.1_ViralProj14284" \
       | sort -u > taxId.asmId.refseq.ready

join -v 1 -1 2 <(sort -k2,2  taxId.asmId.refseq.reference.list) \
   refseq.suppressed.asmId.list | awk '{printf "%d\t%s\n", $2, $1}' \
     | egrep -v "GCF_016700215.2_bGalGal1.pat.whiteleghornlayer.GRCg7w_WZ|Guppy_female_1.0\+MT|GCF_000842245.1_ViralProj14284" \
       | sort -u > taxId.asmId.refseq.reference.ready

# find out what is left over after all the other groups have been
# extracted

join -t$'\t' taxId.asmId.refseq.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) \
     | egrep -v -w "Viridiplantae|Metazoa|Euglenozoa|Alveolata|Amoebozoa|Stramenopiles|Parabasalia|Heterolobosea|Metamonada|Archaea|Vertebrata|Primates|Mammalia|Aves|Percomorphaceae|Osteoglossocephalai" \
       | egrep -v "Opisthokonta;Fungi|Viruses;|Bacteria;" \
          | cut -f2 | sort -u > all.refseq.leftOvers.today

join -t$'\t' taxId.asmId.refseq.reference.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) | grep "Bacteria;" \
     | cut -f2 | sort -u > all.refseq.bacteria.today

join -t$'\t' taxId.asmId.refseq.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) | grep "Viruses;" \
     | cut -f2 | sort -u > all.refseq.viral.today

join -t$'\t' taxId.asmId.refseq.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) | grep -w Viridiplantae \
     | cut -f2 | sort -u > all.refseq.plants.today

join -t$'\t' taxId.asmId.refseq.ready \
  <(sort ../refseq/allAssemblies.taxonomy.tsv) | egrep -w "Metazoa|Euglenozoa|Alveolata|Amoebozoa|Stramenopiles|Parabasalia|Heterolobosea|Metamonada" \
	| grep -v -w "Vertebrata" | cut -f2 | sort -u \
         | sed -e 's/GCF_000507365.1_N__americanus_v1/GCF_000507365.1_N_americanus_v1/;' \
	    > all.refseq.invertebrate.today

join -t$'\t' taxId.asmId.refseq.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) | grep -w Archaea \
     | cut -f2 | sort -u > all.refseq.archaea.today

join -t$'\t' taxId.asmId.refseq.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) | grep "Opisthokonta;Fungi" \
     | cut -f2 | sed -e 's/GCF_000721785.1_Aureobasidium_pullulans_var._pullulans_EXF-150,_assembly_version_1.0/GCF_000721785.1_Aureobasidium_pullulans_var._pullulans_EXF-150_assembly_version_1.0/; s/GCF_000281105.1_Coni_apol_CBS100218__V1/GCF_000281105.1_Coni_apol_CBS100218_V1/;' | sort -u > all.refseq.fungi.today

join -t$'\t' taxId.asmId.refseq.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) | grep -w Vertebrata \
     | cut -f2 | sort -u > all.refseq.verts.today

join -t$'\t' taxId.asmId.refseq.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) | grep -w Primates \
     | cut -f2 | sort -u > all.refseq.primates.today

join -t$'\t' taxId.asmId.refseq.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) | grep -w Mammalia \
      | grep -v -w Primates \
     | cut -f2 | egrep -v "GCF_003121395.1_UOA_WB_1" | sort -u \
       > all.refseq.mammals.today

join -t$'\t' taxId.asmId.refseq.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) | grep -w Aves \
     | cut -f2 | sort -u > all.refseq.birds.today

join -t$'\t' taxId.asmId.refseq.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) \
     | egrep -w 'Percomorphaceae|Osteoglossocephalai' \
       | cut -f2 | sort -u > all.refseq.fish.today

join -t$'\t' taxId.asmId.refseq.ready \
   <(sort ../refseq/allAssemblies.taxonomy.tsv) | grep -w Vertebrata \
   | egrep -v -w 'Percomorphaceae|Osteoglossocephalai|Aves|Mammalia|Primates' \
       | cut -f2 | sort -u > all.refseq.vertebrate.today

grep -v "^#" ~/kent/src/hg/makeDb/doc/asmHubs/master.run.list \
  | awk '{print $2}' | sort -u > hubs.asmId.done.list

./refseqToDo.sh

printf "# summary of vertebrate\n"
printf "# totals done\ttoDo\tclade\n"

export totalToday=0
export hubsDoneToday=0
export toDoToday=0

for S in primates mammals birds fish vertebrate plants invertebrate fungi archaea viral bacteria
do
   todayCount=`cat all.refseq.${S}.today | wc -l`
   hubsDone=`comm -12 all.refseq.${S}.today hubs.asmId.done.list | wc -l`
   toDo=`echo $todayCount $hubsDone | awk '{printf "%d", $1-$2}'`
   totalToday=`echo $totalToday $todayCount | awk '{printf "%d", $1+$2}'`
   hubsDoneToday=`echo $hubsDoneToday $hubsDone | awk '{printf "%d", $1+$2}'`
   toDoToday=`echo $toDoToday $toDo | awk '{printf "%d", $1+$2}'`
   printf "%5d\t%5d\t%5d\t%s\n" "${todayCount}" "${hubsDone}" "${toDo}" "${S}"
done
printf "%5d\t%5d\t%5d\t%s\n" "${totalToday}" "${hubsDoneToday}" "${toDoToday}" "all"

for S in primates mammals birds fish vertebrate plants invertebrate fungi viral bacteria
do
  toDoCount=`comm -23 all.refseq.${S}.today hubs.asmId.done.list | grep -v "GCF_003121395.1_UOA_WB_1" | wc -l`
  if [ "${toDoCount}" -gt 0 ]; then
    printf "#### %s #### $toDoCount to be done, sample of 2: ####\n" "${S}"
    comm -23 all.refseq.${S}.today hubs.asmId.done.list | head -2
  else
    printf "#### %s #### $toDoCount to be done (complete)\n" "${S}"
  fi
done
