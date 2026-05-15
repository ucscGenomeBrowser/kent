#!/bin/bash

if [ ! -s "coverage/order.list" ]; then
  printf "ERROR: do not find the 'coverage/order.list' here ?\n" 1>&2
  exit 255
fi

export names="$HOME/kent/src/hg/makeDb/doc/vgp577way//577.acc.sciName.comName.clade.tsv"
export mkTdb="$HOME/kent/src/hg/makeDb/doc/vgp577way/vgp577way.pl"
export mkHtml="$HOME/kent/src/hg/makeDb/doc/vgp577way/mkHtml.pl"
### this does not work for the hg38, hs1 and mm39 assemblies
export target=`pwd -P | sed -e "s#.*/GC\([AF]\)_#GC\\1_#;" | cut -d"_" -f1-2`
if [[ "${target}" == GC* ]]; then
  ${mkTdb} ${target} "${names}" coverage/order.list > vgp577way.trackDb.txt
  ${mkHtml} ${target} "${names}" coverage/order.list > vgp577way.html
else
   # for hg38, hs1 and mm39 in /hive/data/genomes/<db>/...
   target=`pwd -P | cut -d'/' -f5`
   ${mkTdb} ${target} "${names}" coverage/order.list > vgp577way.trackDb.txt
   ${mkHtml} ${target} "${names}" coverage/order.list > vgp577way.html
fi
