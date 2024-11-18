#!/bin/bash

cd /hive/data/outside/ncbi/genomes/reports/newAsm

for S in primates mammals birds fish vertebrate plants fungi invertebrate viral
do
  TS=`date "+%s"`
#  printf "# working %s\t%s\n" "${S}" "${TS}" 1>&2

# the tr fixes up names that have characters in them that will cause
# great trouble on the command line.  It isn't clear if the substitute
# of the underscore is good enough at this time.
sort all.refseq.${S}.today \
  | awk -F$'_' '{
    printf "%s_%s\t", $1, $2
    for (i = 3; i <= NF; ++i) {
       printf "_%s", $i
    }
    printf "\n"
  }' | join -t$'\t' - <(sort ../assembly_summary_refseq.txt) \
    | awk -v S="${S}" -F$'\t' '{ gsub(" ", "_", $9); printf "runBuild %s%s %s %s\t%s\n", $1, $2, S, $9, $16}' \
       | egrep -v "GCF_009914755.1_T2T-CHM13v2.0|GCF_003121395.1_UOA_WB_1|GCF_016700215.2_bGalGal1.pat.whiteleghornlayer.GRCg7w|GCF_000001405.40_GRCh38.p14" | tr '=`,:%+/#>!;"' '_' | tr -d '][()*' | tr -d "'" | sed -e 's/__/_/g;' | sed -e 's#^#./#;' \
   | join -v2 -2 2 hubs.asmId.done.list - \
     | awk '{printf "%s %s %s %s\t%s\n", $2, $1, $3, $4, $5, $6}' \
        > rs.todo.${S}.txt
done

for S in archaea bacteria
do
  TS=`date "+%s"`
#  printf "# working %s\t%s\n" "${S}" "${TS}" 1>&2

# the tr fixes up names that have characters in them that will cause
# great trouble on the command line.  It isn't clear if the substitute
# of the underscore is good enough at this time.
sort all.refseq.${S}.today \
  | awk -F$'_' '{
    printf "%s_%s\t", $1, $2
    for (i = 3; i <= NF; ++i) {
       printf "_%s", $i
    }
    printf "\n"
  }' | join -t$'\t' - <(egrep -w "reference genome|representative genome" ../assembly_summary_refseq.txt | sort) \
    | awk -v S="${S}" -F$'\t' '{ gsub(" ", "_", $9); printf "runBuild %s%s %s %s\t%s\n", $1, $2, S, $9, $16}' \
       | egrep -v "GCF_003121395.1_UOA_WB_1|GCF_016700215.2_bGalGal1.pat.whiteleghornlayer.GRCg7w|GCF_000001405.40_GRCh38.p14" | tr '=`,:%+/#>!;"' '_' | tr -d '][()*' | tr -d "'" | sed -e 's/__/_/g;' | sed -e 's#^#./#;' \
   | join -v2 -2 2 hubs.asmId.done.list - \
     | awk '{printf "%s %s %s %s\t%s\n", $2, $1, $3, $4, $5, $6}' \
        > rs.todo.${S}.txt
done

# TS=`date "+%s"`
# printf "# complete %s\n" "${TS}" 1>&2


# TS=`date "+%s"`
# printf "# complete %s\n" "${TS}" 1>&2

exit $?

##  older procedure, much much slower, over 11 hours vs 4 seconds

for S in primates mammals birds fish vertebrate plants fungi invertebrate archaea viral bacteria
do
  TS=`date "+%s"`
  printf "# working %s\t%s\n" "${S}" "${TS}" 1>&2
  join -v1 all.refseq.${S}.today hubs.asmId.done.list | while read asmId
  do
    export accessionId=`echo $asmId | awk -F'_' '{printf "%s_%s\n", $1,$2}'`
    export sciName=`grep -w "${asmId}" ../assembly_summary_refseq.txt | cut -f8 | sed -e 's/ /_/g;'`
    export asmDate=`grep -w "${accessionId}" ../assembly_summary_refseq.txt | cut -f15`
    printf "./runBuild %s %s %s %s\t%s\n" "${accessionId}" "${asmId}" "${S}" "${sciName}" "${asmDate}"
  done | grep -v "GCF_003121395.1_UOA_WB_1" > rs.todo.${S}.txt
done
TS=`date "+%s"`
printf "# complete %s\n" "${TS}" 1>&2

exit $?

all.refseq.archaea.today        all.refseq.mammals.today
all.refseq.bacteria.today       all.refseq.plants.today
all.refseq.birds.today          all.refseq.primates.today
all.refseq.fish.today           all.refseq.vertebrate.today
all.refseq.fungi.today          all.refseq.verts.today
all.refseq.invertebrate.today  all.refseq.viral.today
all.refseq.leftOvers.today

-rw-rw-r-- 1    4013 Dec  6 14:47 all.refseq.leftOvers.today
-rw-rw-r-- 1 6649930 Dec  6 14:47 all.refseq.bacteria.today
-rw-rw-r-- 1  364615 Dec  6 14:47 all.refseq.viral.today
-rw-rw-r-- 1    4123 Dec  6 14:47 all.refseq.plants.today
-rw-rw-r-- 1    7845 Dec  6 14:47 all.refseq.invertebrate.today
-rw-rw-r-- 1   35280 Dec  6 14:47 all.refseq.archaea.today
-rw-rw-r-- 1   11860 Dec  6 14:47 all.refseq.fungi.today
-rw-rw-r-- 1   12818 Dec  6 14:47 all.refseq.verts.today
-rw-rw-r-- 1     839 Dec  6 14:47 all.refseq.primates.today
-rw-rw-r-- 1    4199 Dec  6 14:47 all.refseq.mammals.today
-rw-rw-r-- 1    2716 Dec  6 14:47 all.refseq.birds.today
-rw-rw-r-- 1    3445 Dec  6 14:47 all.refseq.fish.today
-rw-rw-r-- 1    1619 Dec  6 14:47 all.refseq.vertebrate.today


total 170377
-rw-rw-r-- 1   559682 Oct 13 12:29 refseq.taxonomy.taxId
-rw-rw-r-- 1   549459 Oct 13 12:29 asmSummary.taxId
-rwxrwxr-x 1      386 Oct 13 12:36 refSeqTaxIds.sh
-rw-rw-r-- 1   952459 Oct 13 12:36 asmSummary.category.taxId
-rw-rw-r-- 1   952459 Oct 13 12:36 asmSummary.species.taxId
-rw-rw-r-- 1     9389 Oct 13 13:58 all.refseq.verts.today.0
-rw-rw-r-- 1  7413274 Oct 13 14:14 rs.l
-rw-rw-r-- 1   134761 Oct 13 14:17 t
-rw-rw-r-- 1     9168 Oct 13 14:20 refseq.verts.today
-rwxrwxr-x 1     2535 Oct 13 14:39 refseq.sh
-rwxrwxr-x 1     2666 Oct 13 15:21 genbank.sh
-rw-rw-r-- 1 30248792 Oct 14 02:14 taxId.asmId.genbank.list
-rw-rw-r-- 1   215101 Oct 14 02:14 genbank.suppressed.asmId.list
-rw-rw-r-- 1 30248792 Oct 14 02:14 taxId.asmId.genbank.ready
-rw-rw-r-- 1    41010 Oct 14 02:14 all.genbank.verts.today
-rw-rw-r-- 1    10317 Oct 14 02:14 all.genbank.primates.today
-rw-rw-r-- 1    14588 Oct 14 02:14 all.genbank.mammals.today
-rw-rw-r-- 1     5462 Oct 14 02:14 all.genbank.birds.today
-rw-rw-r-- 1     8532 Oct 14 02:14 all.genbank.fish.today
-rw-rw-r-- 1     2142 Oct 14 02:14 all.genbank.vertebrates.today
-rw-rw-r-- 1  7418893 Oct 14 15:07 taxId.asmId.refseq.list
-rw-rw-r-- 1   521967 Oct 14 15:07 refseq.suppressed.asmId.list
-rw-rw-r-- 1  7418893 Oct 14 15:07 taxId.asmId.refseq.ready
-rw-rw-r-- 1     9168 Oct 14 15:07 all.refseq.verts.today
-rw-rw-r-- 1      820 Oct 14 15:07 all.refseq.primates.today
-rw-rw-r-- 1     3130 Oct 14 15:07 all.refseq.mammals.today
-rw-rw-r-- 1     2149 Oct 14 15:07 all.refseq.birds.today
-rw-rw-r-- 1     2219 Oct 14 15:07 all.refseq.fish.today
-rw-rw-r-- 1      881 Oct 14 15:07 all.refseq.vertebrates.today
-rw-rw-r-- 1    13497 Oct 14 15:07 hubs.asmId.done.list
-rw-rw-r-- 1        0 Oct 14 15:52 refseqToDo.sh
