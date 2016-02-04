#!/bin/bash

set -beEu -o pipefail
# set -x

function usage() {
  printf "usage: mkPartitionedHubIndex.sh [ncbi|ucsc] [viral|bacteria] [genbank|refseq]\n" 1>&2
  printf "select to construct ucsc or ncbi groups html index files\n" 1>&2
  printf "select viral or bacteria hierarchies to partition\n" 1>&2
  printf "also select refseq or genbank hierarchy\n" 1>&2
}

if [ $# -ne 3 ]; then
  usage
  exit 255
fi

export ncbiUcsc=$1
export subType=$2
export refseqGenbank=$3

if [ "${subType}" != "viral" -a "${subType}" != "bacteria" ]; then
  printf "ERROR: select viral or bacteria assemblies update\n" 1>&2
  usage
  exit 255
fi

printf "running subType: %s\n" "${subType}" 1>&2

export fileSuffix=""

if [ "${ncbiUcsc}" = "ncbi" ]; then
  fileSuffix=".ncbi"
fi

cd /hive/data/inside/ncbi/genomes/$refseqGenbank/hubs

export dateStamp=`date "+%FT%T %s"`

declare -A pageTitles

pageTitles=( ["bacteria"]="Bacterial" \
  ["viral"]="Viral" )

pageTitle="${pageTitles["$subType"]}"

export inside="/hive/data/inside/ncbi/genomes/$refseqGenbank"

printf "%s\n" \
'<p>Collection of hubs for '${pageTitle}' genome assemblies.
Use these links to go to the index for that subset of '${subType}' assemblies:
<table class="sortable" border="1"><tr><thead>
  <th>'${subType}'<br>subset</th>
  <th>number&nbsp;of<br>species</th>
  <th>number&nbsp;of<br>assemblies</th>
  <th>total&nbsp;contig<br>count</th>
  <th>total&nbsp;nucleotide<br>count</th>
  <th>average<br>contig&nbsp;size</th>
  <th>average<br>assembly&nbsp;size</th>
</tr></thead><tbody>'

export sumOrgCount=0
export sumAsmCount=0
export sumContigCount=0
export sumTotalSize=0

for subDir in ${subType}/??
do

bacPartName=`echo ${subDir} | sed -e 's#/#.#;'`
hubLink="$refseqGenbank/${subDir}"

orgCount=`cut -f4 ${subDir}/${subType}.??.order${fileSuffix}.tab | sort -u | wc -l`
asmCount=`cut -f6 ${subDir}/${subType}.??.order${fileSuffix}.tab | sort -u | wc -l`

totalContigs=`cut -f1 ${subDir}/${subType}.??.total.stats${fileSuffix}.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
totalSize=`cut -f2 ${subDir}/${subType}.??.total.stats${fileSuffix}.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
aveContigSize=$(( totalSize/totalContigs ))
aveAsmSize=$(( totalSize/asmCount ))
sumOrgCount=$(( sumOrgCount+orgCount ))
sumAsmCount=$(( sumAsmCount+asmCount ))
sumContigCount=$(( sumContigCount+totalContigs ))
sumTotalSize=$(( sumTotalSize+totalSize ))
export groupLabel="${subDir}"

LC_NUMERIC=en_US /usr/bin/printf "<tr><td align=right><a href=\"/gbdb/hubs/%s/%s%s.html\">%s</a></td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
</tr>\n" "${hubLink}" "${bacPartName}" "${fileSuffix}" "$groupLabel" "$orgCount" "$asmCount" "$totalContigs" "$totalSize" "${aveContigSize}" "${aveAsmSize}"

done
#### for each directory in ${subType}/??

aveContigSize=$(( sumTotalSize/sumContigCount ))
aveAsmSize=$(( sumTotalSize/sumAsmCount ))

LC_NUMERIC=en_US /usr/bin/printf "</tbody><tfoot><tr>
  <td colspan=\"2\" align=right>totals:</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
</tr></tfoot>
</table>\n" $sumOrgCount $sumAsmCount $sumContigCount $sumTotalSize "${aveContigSize}" "${aveAsmSize}"
# LC_NUMERIC=en_US /usr/bin/printf "%s\t%s\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\n" "${dateStamp}" "totals" $sumOrgCount $sumAsmCount $sumContigCount $sumTotalSize "${aveContigSize}" "${aveAsmSize}" >> logs/assembly.counts.txt
