#!/bin/bash

# set -x
cd /hive/data/inside/ncbi/genomes/refseq/hubs

export dateStamp=`date "+%FT%T %s"`

function usage() {
  printf "usage: ncbiPartitioned.sh <viral|bacteria>\n" 1>&2
  printf "select viral or bacteria hierarchies to partition\n" 1>&2
}

if [ $# -ne 1 ]; then
  usage
  exit 255
fi

export subType=$1

if [ "${subType}" != "viral" -a "${subType}" != "bacteria" ]; then
  printf "ERROR: select viral or bacteria assemblies update\n" 1>&2
  usage
  exit 255
fi

printf "running subType: %s\n" "${subType}" 1>&2

declare -A pageTitles

pageTitles=( ["bacteria"]="Bacterial" \
  ["viral"]="Viral" )

pageTitle="${pageTitles["$subType"]}"

export inside="/hive/data/inside/ncbi/genomes/refseq"

printf "%s\n" \
'<p>Collection of hubs for '${pageTitle}' genome assemblies.
Use these links to go to the index for that subset of '${subType}' assemblies:
<table border=1><tr>
  <th>'${subType}'<br>subset</th>
  <th>number&nbsp;of<br>species</th>
  <th>number&nbsp;of<br>assemblies</th>
  <th>total&nbsp;contig<br>count</th>
  <th>total&nbsp;nucleotide<br>count</th>
  <th>average<br>contig&nbsp;size</th>
  <th>average<br>assembly&nbsp;size</th>
</tr>'

export sumOrgCount=0
export sumAsmCount=0
export sumContigCount=0
export sumTotalSize=0

for subDir in ${subType}/??
do

bacPartName=`echo ${subDir} | sed -e 's#/#.#;'`
hubLink="refseq/${subDir}"

orgCount=`cut -f4 ${subDir}/${subType}.??.order.tab | sort -u | wc -l`
asmCount=`cut -f6 ${subDir}/${subType}.??.order.tab | sort -u | wc -l`

totalContigs=`cut -f1 ${subDir}/${subType}.??.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
totalSize=`cut -f2 ${subDir}/${subType}.??.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
aveContigSize=$(( totalSize/totalContigs ))
aveAsmSize=$(( totalSize/asmCount ))
sumOrgCount=$(( sumOrgCount+orgCount ))
sumAsmCount=$(( sumAsmCount+asmCount ))
sumContigCount=$(( sumContigCount+totalContigs ))
sumTotalSize=$(( sumTotalSize+totalSize ))
export groupLabel="${subDir}"

LC_NUMERIC=en_US /usr/bin/printf "<tr><td align=right><a href=\"http://genome-test.soe.ucsc.edu/~hiram/hubs/%s/%s.ncbi.html\">%s</a></td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
</tr>" "${hubLink}" "${bacPartName}" "$groupLabel" "$orgCount" "$asmCount" "$totalContigs" "$totalSize" "${aveContigSize}" "${aveAsmSize}"

done
#### for each directory in ${subType}/??

aveContigSize=$(( sumTotalSize/sumContigCount ))
aveAsmSize=$(( sumTotalSize/sumAsmCount ))

LC_NUMERIC=en_US /usr/bin/printf "<tr><td align=right>totals:</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
</tr>
</table>\n" $sumOrgCount $sumAsmCount $sumContigCount $sumTotalSize "${aveContigSize}" "${aveAsmSize}"
# LC_NUMERIC=en_US /usr/bin/printf "%s\t%s\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\n" "${dateStamp}" "totals" $sumOrgCount $sumAsmCount $sumContigCount $sumTotalSize "${aveContigSize}" "${aveAsmSize}" >> logs/assembly.counts.txt
