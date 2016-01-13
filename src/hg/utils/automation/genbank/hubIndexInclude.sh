#!/bin/bash

set -beEu -o pipefail
# set -x

if [ $# -lt 2 ]; then
  printf "usage: hubIndexInclude.sh [ncbi|ucsc] [refseq|genbank] [tableOnly]\n" 1>&2
  printf "select to construct ucsc or ncbi groups html index files\n" 1>&2
  printf "also select refseq or genbank hierarchy\n" 1>&2
  printf "optionally tableOnly\n" 1>&2
  exit 255
fi

export ncbiUcsc=$1
export refseqGenbank=$2
export tableOnly=0
if [ $# -eq 3 ]; then
  tableOnly=1
fi

export fileSuffix=""

if [ "${ncbiUcsc}" = "ncbi" ]; then
  fileSuffix=".ncbi"
fi

cd /hive/data/inside/ncbi/genomes/${refseqGenbank}/hubs
mkdir -p logs

export dateStamp=`date "+%FT%T %s"`

export inside="/hive/data/inside/ncbi/genomes/${refseqGenbank}"

case $refseqGenbank in
  genbank)
     printf "%s\n" '<p>Collection of hubs for Genbank genome assemblies.'
     ;;
  refseq)
     printf "%s\n" '<p>Collection of hubs for RefSeq genome assemblies.'
     ;;
esac

printf "%s\n" \
'Use these links to go to the index for that subset of assemblies:
<table class="sortable" border="1"><thead><tr>
  <th>species<br>subset</th>
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

export genbankList="other vertebrate_other vertebrate_mammalian plant protozoa invertebrate archaea bacteria"
if [ $refseqGenbank = "refseq" ]; then
   genbankList="plant protozoa invertebrate vertebrate_other vertebrate_mammalian fungi archaea viral bacteria"
fi

# for groupName in archaea bacteria fungi invertebrate plant protozoa other vertebrate_mammalian vertebrate_other
# for groupName in plant protozoa invertebrate vertebrate_other vertebrate_mammalian fungi archaea viral bacteria
for groupName in ${genbankList}
do

hubLink="${refseqGenbank}/${groupName}"

orgCount=`cut -f4 ${groupName}.order${fileSuffix}.tab | sort -u | wc -l`
asmCount=`cut -f6 ${groupName}.order${fileSuffix}.tab | sort -u | wc -l`

totalContigs=`cut -f1 ${groupName}/${groupName}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
totalSize=`cut -f2 ${groupName}/${groupName}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
aveContigSize=$(( totalSize/totalContigs ))
aveAsmSize=$(( totalSize/asmCount ))
sumOrgCount=$(( sumOrgCount+orgCount ))
sumAsmCount=$(( sumAsmCount+asmCount ))
sumContigCount=$(( sumContigCount+totalContigs ))
sumTotalSize=$(( sumTotalSize+totalSize ))
export groupLabel="${groupName}"
if [ "${groupName}" = "other" ]; then
   groupLabel="${groupName}/synthetic assemblies"
fi

LC_NUMERIC=en_US /usr/bin/printf "<tr><td align=right><a href=\"/gbdb/hubs/%s/%s%s.html\">%s</a></td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
</tr>" "${hubLink}" "${groupName}" "${fileSuffix}" "$groupLabel" $orgCount $asmCount $totalContigs $totalSize "${aveContigSize}" "${aveAsmSize}"
LC_NUMERIC=en_US /usr/bin/printf "%s\t%s\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\n" "${dateStamp}" "${groupName}" $orgCount $asmCount $totalContigs $totalSize "${aveContigSize}" "${aveAsmSize}" >> logs/assembly.counts.txt

done

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
</tr></tfoot>\n" $sumOrgCount $sumAsmCount $sumContigCount $sumTotalSize "${aveContigSize}" "${aveAsmSize}"
LC_NUMERIC=en_US /usr/bin/printf "%s\t%s\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\n" "${dateStamp}" "totals" $sumOrgCount $sumAsmCount $sumContigCount $sumTotalSize "${aveContigSize}" "${aveAsmSize}" >> logs/assembly.counts.txt

printf "%s\n" '</table>'

if [ $tableOnly -eq 1 ]; then
   exit 0
fi

printf "%s\n" \
'<p>
<b>NOTE:</b> This is a prototype work in progress.  Not all assemblies are
represented here yet.  Prototype gene tracks from the NCBI gene predictions
delivered with the assembly are available on most assemblies.
There are no blat servers.  Users could copy the hub skeleton structure
of a specific assembly to local systems and run a blat server at
their location with their own assembly hub of that specific
genome.  This system may not be available at all times as the
procedures are finalized of how to keep this up to date on
an automatic process.
</p>
<p>
The "Taxon ID" link will go to the Entrez taxonomy for that ID.<br>
The "common name" link will go to the UCSC genome browser for that assembly.<br>
The "biosample" link will go to the Entrez biosample for that ID.<br>
The "accession" link will go to the Entrez assembly for that ID.<br>
The "assembly" link will go to the NCBI Genbank FTP source directory.
</p>'
