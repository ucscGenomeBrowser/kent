#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "usage: rebuild.sh makeItSo\n" 1>&2
  exit 255
fi

if [ "$1" != "makeItSo" ]; then
  printf "usage: rebuild.sh makeItSo\n" 1>&2
  exit 255
fi

export srcDir="$HOME/kent/src/hg/gar"
export dateStamp=`date "+%F"`
export workDir="/hive/data/outside/ncbi/genomes/gar/$dateStamp"

if [ ! -d "${workDir}" ]; then
  mkdir "${workDir}"
fi

printf "$workDir\n"
cd "${workDir}"
printf "### running time ucscEquiv.sh ###\n"
time $srcDir/ucscEquiv.sh
printf "### running time garTable.sh ###\n"
time $srcDir/garTable.sh > next.html 2> ${dateStamp}.log
rm -f index.html
mv next.html index.html
chmod +x index.html
rm -f assemblyTable.txt requestTable.txt
for F in `ls -rt *.tableData.txt`
do
   awk -F$'\t' '$2 == "view"' "${F}" | cut -f1,3- >> assemblyTable.txt
   awk -F$'\t' '$2 == "request"' "${F}" | cut -f1,3- >> requestTable.txt
done
scp -p assemblyTable.txt requestTable.txt qateam@hgdownload:/mirrordata/hubs/
printf "### finished ###\n"
printf "# %s/index.html\n" "${workDir}"
