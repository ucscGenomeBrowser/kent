#!/bin/bash

cd /hive/data/outside/ncbi/genomes/reports/newAsm

for S in primates mammals birds fish vertebrate plants fungi invertebrate archaea viral bacteria
do
  TS=`date "+%s"`
#  printf "# working %s\t%s\n" "${S}" "${TS}" 1>&2

sort all.genbank.${S}.today \
  | awk -F$'_' '{
    printf "%s_%s\t", $1, $2
    for (i = 3; i <= NF; ++i) {
       printf "_%s", $i
    }
    printf "\n"
  }' | join -t$'\t' - <(sort ../assembly_summary_genbank.txt) \
    | awk -v S="${S}" -F$'\t' '{ gsub(" ", "_", $9); printf "runBuild %s%s %s %s\t%s\n", $1, $2, S, $9, $16}' \
       | tr '=`,:%+/#>!;"' '_' | tr -d '][()*' | tr -d "'" | sed -e 's/__/_/g;' \
         | sed -e 's#^#./#;' \
   | join -v2 -2 2 hubs.asmId.done.list - \
     | awk '{printf "%s %s %s %s\t%s\n", $2, $1, $3, $4, $5, $6}' \
        > gb.todo.${S}.txt
done
# the tr fixes up names that have characters in them that will cause
# great trouble on the command line.  It isn't clear if the substitute
# of the underscore is good enough at this time.

# TS=`date "+%s"`
# printf "# complete %s\n" "${TS}" 1>&2

exit $?

##  older procedure, much much slower, over 11 hours vs 4 seconds
#!/bin/bash

cd /hive/data/outside/ncbi/genomes/reports/newAsm

# all.genbank.birds.today         gb.todo.primates.txt.0
# all.genbank.fish.today          gb.todo.verts.txt
# all.genbank.mammals.today       genbank.sh
# all.genbank.primates.today      genbank.suppressed.asmId.list
# all.genbank.vertebrates.today   genbankToDo.sh
# all.genbank.verts.today         hubs.

# for S in invertebrate
# for S in vertebrate plants fungi invertebrate
for S in primates mammals birds fish vertebrate plants fungi invertebrate archaea viral bacteria
do
  printf "# running %s\n" "${S}" 1>&2
  join -v1 all.genbank.${S}.today hubs.asmId.done.list | while read asmId
  do
    export accessionId=`echo "${asmId}" | awk -F'_' '{printf "%s_%s\n", $1,$2}'`
    export asmDate=`grep -w "${accessionId}" ../assembly_summary_genbank.txt | cut -f15`
    export sciName=`grep -w "${accessionId}" ../assembly_summary_genbank.txt | cut -f8 | sed -e 's/ /_/g;'`
    printf "./runBuild %s %s %s %s\t%s\n" "${accessionId}" "${asmId}" "${S}" "${sciName}" "${asmDate}"
  done > gb.todo.$S.txt
done

# all.genbank.archaea.today
# all.genbank.bacteria.today
# all.genbank.birds.today
# all.genbank.fish.today
# all.genbank.fungi.today
# all.genbank.invertebrate.today
# all.genbank.leftOvers.today
# all.genbank.mammals.today
# all.genbank.plants.today
# all.genbank.primates.today
# all.genbank.vertebrate.today
# all.genbank.verts.today
# all.genbank.viral.today
