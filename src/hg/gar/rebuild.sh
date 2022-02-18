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
time $srcDir/garTable.sh > index.html 2> ${dateStamp}.log
chmod +x index.html
printf "### finished ###\n"
