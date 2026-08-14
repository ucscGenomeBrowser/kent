#!/bin/bash

if [ $# != 1 ]; then
  printf "usage: mkIRowsJL.sh <fullPath.to.2bit> > jobList\n" 1>&2
  exit 255
fi

export twoBit="${1}"
if [ ! -s "${twoBit}" ]; then
  printf "ERROR: can not find the given twoBit file:\n%s\n" "${twoBit}" 1>&2
fi

for M in `shuf maf.list`
do
  chrMaf=`basename "${M}"`
  printf "mafAddIRowsStream -nBeds=nBeds ${M} ${twoBit} result/${chrMaf}\n"
done
