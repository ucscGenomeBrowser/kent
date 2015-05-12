#!/bin/bash

export LANG=C

export target=$1
export query=$2
export name=$3

export selected="selected.$name.list"

if [ -s $selected ]; then
   echo 'join -t "	" '"selected.${name}.list ${target}.genes.gp | cut -f1,3- > ${target}.selected.${name}.gp" 1>&2
   join -t "	" selected.${name}.list \
      ${target}.genes.gp | cut -f1,3- \
        > ${target}.selected.${name}.gp
   cut -f2 selected.${name}.list | while read Q
do
 egrep "^${Q}	" ${query}.genes.gp
done > ${query}.selected.${name}.gp
   awk '{printf "%s\t%d\t%d\t%s\t%s\n", $2,$4,$5,$1,$3}' ${target}.selected.${name}.gp > ${target}.selected.${name}.bed
   awk '{printf "%s\t%d\t%d\t%s\t%s\n", $2,$4,$5,$1,$3}' ${query}.selected.${name}.gp > ${query}.selected.${name}.bed
   ./adjustSizes.pl $target $query $name
   twoBitToFa -noMask -bed=${target}.toFetch.${name}.bed \
      /hive/data/genomes/${target}/${target}.unmasked.2bit \
         ${target}.selected.${name}.fa
   twoBitToFa -noMask -bed=${query}.toFetch.${name}.bed \
      /hive/data/genomes/${query}/${query}.unmasked.2bit \
         ${query}.selected.${name}.fa
   echo ">$target" > ${target}.${name}.fa
   grep -v "^>" ${target}.selected.${name}.fa \
      >> ${target}.${name}.fa
   echo ">$query" > ${query}.${name}.fa
   grep -v "^>" ${query}.selected.${name}.fa \
      >> ${query}.${name}.fa
   echo "#!/bin/bash" > tune.${name}.sh
   echo "set -beEu -o pipefail" >> tune.${name}.sh
   echo "export LANG=C" >> tune.${name}.sh
   echo "lastz_D --inferonly=~/kent/src/hg/utils/automation/lastz_D/createScoresFileControl.txt \\
     $target.$name.fa $query.$name.fa \\
       | ~/kent/src/hg/utils/automation/lastz_D/expandScoresFile.py \\
       > ${target}.${query}.tuning.${name}.txt 2>&1" \
     >> tune.${name}.sh
   chmod +x tune.${name}.sh
else
  echo "selectedFasta.sh <target> <query> <topN>"
  exit 255
fi
