#!/bin/bash

export gbkRseq="genbank"

cd /hive/data/outside/ncbi/genomes/reports/newAsm

grep -v "^#" ../assembly_summary_genbank.txt \
   | awk -F$'\t' '{printf "%d\t%s_%s\n", $6,$1,$16}' \
    | egrep -v "Guppy_female_1.0\+MT" \
      | sed -e 's/ /_/g;' | sort > taxId.asmId.${gbkRseq}.list

grep -w suppressed ../assembly_summary_${gbkRseq}_historical.txt | cut -f1,16 \
    | sed -e 's/[ 	]/_/g;' | sort -u > ${gbkRseq}.suppressed.asmId.list

join -v 1 -1 2 <(sort -k2,2  taxId.asmId.${gbkRseq}.list) \
   ${gbkRseq}.suppressed.asmId.list | awk '{printf "%d\t%s\n", $2, $1}' \
     | sort -u > taxId.asmId.${gbkRseq}.ready

# find out what is left over after all the other groups have been
# extracted

# cut -d';' -f1-4 33.todo | sort | uniq -c
#       1 Eukaryota;Heterolobosea;Schizopyrenida;Vahlkampfiidae
#       2 Eukaryota;Metamonada;Fornicata;Diplomonadida
#       2 Eukaryota;Metamonada;Parabasalia;Trichomonadida
#       1 Eukaryota;Metamonada;Parabasalia;Tritrichomonadida
#       7 Eukaryota;Opisthokonta;Fungi;Dikarya
#       3 Eukaryota;Opisthokonta;Fungi;Fungi incertae sedis
#       1 Eukaryota;Opisthokonta;Metazoa;Eumetazoa
#       1 Eukaryota;Parabasalia;Trichomonadida;Trichomonadidae
#       1 Eukaryota;Parabasalia;Tritrichomonadida;Tritrichomonadidae
#       8 Eukaryota;Sar;Stramenopiles;Oomycetes
#       2 Eukaryota;Sar;Stramenopiles;Oomycota
#       2 Eukaryota;Stramenopiles;Oomycetes;Peronosporales
#       2 Eukaryota;Stramenopiles;Oomycetes;Pythiales


join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) \
     | egrep -v -w "Viridiplantae|Metazoa|Euglenozoa|Alveolata|Amoebozoa|Stramenopiles|Parabasalia|Heterolobosea|Metamonada|Archaea|Vertebrata|Primates|Mammalia|Aves|Percomorphaceae|Osteoglossocephalai" \
       | egrep -v "Opisthokonta;Fungi|Viruses;|Bacteria;" \
          | cut -f2 | sort -u > all.${gbkRseq}.leftOvers.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) | grep "Bacteria;" \
     | cut -f2 | sort -u > all.${gbkRseq}.bacteria.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) | grep "Viruses;" \
     | cut -f2 | sort -u > all.${gbkRseq}.viral.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) | grep -w Viridiplantae \
     | cut -f2 | sort -u > all.${gbkRseq}.plants.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
  <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) | egrep -w "Metazoa|Euglenozoa|Alveolata|Amoebozoa|Stramenopiles|Parabasalia|Heterolobosea|Metamonada" \
        | grep -v -w "Vertebrata" | cut -f2 | sort -u \
         | sed -e 's/GCA_000507365.1_N__americanus_v1/GCA_000507365.1_N_americanus_v1/; s#GCA_003067545.1_MHOM_LB__2017_IK#GCA_003067545.1_MHOM_LB_2017_IK#; s#GCA_003352575.1_MHOM_LB__2015_IK#GCA_003352575.1_MHOM_LB_2015_IK#;' \
            > all.${gbkRseq}.invertebrate.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) | grep -w Archaea \
     | cut -f2 | sort -u > all.${gbkRseq}.archaea.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) | grep "Opisthokonta;Fungi" \
     | cut -f2 | sed -e 's/GCA_000721785.1_Aureobasidium_pullulans_var._pullulans_EXF-150,_assembly_version_1.0/GCA_000721785.1_Aureobasidium_pullulans_var._pullulans_EXF-150_assembly_version_1.0/; s/GCA_000281105.1_Coni_apol_CBS100218__V1/GCA_000281105.1_Coni_apol_CBS100218_V1/; s/GCA_000441875.1_Bgt_#70_1/GCA_000441875.1_Bgt_70_1/; s#GCA_000507345.1_OVT-YTM/97_Newbler_2.8#GCA_000507345.1_OVT-YTM_97_Newbler_2.8#; s#GCA_000827465.1_Tulasnella_calospora_AL13/4D_v1.0#GCA_000827465.1_Tulasnella_calospora_AL13_4D_v1.0#; s#GCA_000586535.1_Weihenstephan_34/70#GCA_000586535.1_Weihenstephan_34_70#; s#GCA_000969745.1_M__oryzae_MG01_v1#GCA_000969745.1_M_oryzae_MG01_v1#; s#GCA_001049995.1_VPCI_479/P/13#GCA_001049995.1_VPCI_479_P_13#; s#GCA_015852335.1_UCR_MchalNRRL2769__1.0#GCA_015852335.1_UCR_MchalNRRL2769_1.0#;' | sort -u > all.${gbkRseq}.fungi.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) | grep -w Vertebrata \
     | cut -f2 | sort -u > all.${gbkRseq}.verts.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) | grep -w Primates \
     | cut -f2 | sort -u > all.${gbkRseq}.primates.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) | grep -w Mammalia \
      | grep -v -w Primates \
     | cut -f2 | sort -u > all.${gbkRseq}.mammals.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) | grep -w Aves \
     | cut -f2 | sort -u > all.${gbkRseq}.birds.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) \
     | egrep -w 'Percomorphaceae|Osteoglossocephalai' \
       | cut -f2 | sort -u > all.${gbkRseq}.fish.today

join -t$'\t' taxId.asmId.${gbkRseq}.ready \
   <(sort ../${gbkRseq}/allAssemblies.taxonomy.tsv) | grep -w Vertebrata \
   | egrep -v -w 'Percomorphaceae|Osteoglossocephalai|Aves|Mammalia|Primates' \
       | cut -f2 | sort -u > all.${gbkRseq}.vertebrate.today

grep -v "^#" ~/kent/src/hg/makeDb/doc/asmHubs/master.run.list \
  | awk '{print $2}' | sort -u > hubs.asmId.done.list

./genbankToDo.sh

printf "# summary of vertebrates\n"
printf "# totals done\ttoDo\tclade\n"

export totalToday=0
export hubsDoneToday=0
export toDoToday=0

for S in primates mammals birds fish vertebrate plants invertebrate fungi archaea
do
   todayCount=`cat all.${gbkRseq}.${S}.today | wc -l`
   hubsDone=`comm -12 all.${gbkRseq}.${S}.today hubs.asmId.done.list | wc -l`
   toDo=`echo $todayCount $hubsDone | awk '{printf "%d", $1-$2}'`
   totalToday=`echo $totalToday $todayCount | awk '{printf "%d", $1+$2}'`
   hubsDoneToday=`echo $hubsDoneToday $hubsDone | awk '{printf "%d", $1+$2}'`
   toDoToday=`echo $toDoToday $toDo | awk '{printf "%d", $1+$2}'`
   printf "%5d\t%5d\t%5d\t%s\n" "${todayCount}" "${hubsDone}" "${toDo}" "${S}"
done
printf "%5d\t%5d\t%5d\t%s\n" "${totalToday}" "${hubsDoneToday}" "${toDoToday}" "all"

