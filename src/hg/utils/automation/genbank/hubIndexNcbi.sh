#!/bin/bash

# set -x
cd /hive/data/inside/ncbi/genomes/refseq/hubs
mkdir -p logs

export dateStamp=`date "+%FT%T %s"`

declare -A hubLinks

hubLinks=( ["plant"]="gbkPlant" ["fungi"]="gbkFungi" ["other"]="gbkOther" \
  ["archaea"]="gbkArchaea" ["protozoa"]="gbkProtozoa" \
  ["vertebrate_mammalian"]="gbkVertebrateMammalian" \
  ["vertebrate_other"]="gbkVertebrateOther" \
  ["bacteria"]="gbkBacteria" \
  ["invertebrate"]="gbkInvertebrate" )

export inside="/hive/data/inside/ncbi/genomes/refseq"

printf "%s\n" \
'<p>Collection of hubs for RefSeq genome assemblies.
Use these links to go to the index for that subset of assemblies:
<table border=1><tr>
  <th>species<br>subset</th>
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

# for groupName in archaea bacteria fungi invertebrate plant protozoa other vertebrate_mammalian vertebrate_other
for groupName in plant protozoa invertebrate vertebrate_other vertebrate_mammalian fungi archaea viral bacteria
do

# hubLink="${hubLinks["$groupName"]}"
hubLink="refseq/${groupName}"

orgCount=`cut -f4 ${groupName}.order.tab | sort -u | wc -l`
asmCount=`cut -f6 ${groupName}.order.tab | sort -u | wc -l`

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

LC_NUMERIC=en_US /usr/bin/printf "<tr><td align=right><a href=\"http://genome-test.soe.ucsc.edu/~hiram/hubs/%s/%s.ncbi.html\">%s</a></td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
</tr>" "${hubLink}" "${groupName}" "$groupLabel" $orgCount $asmCount $totalContigs $totalSize "${aveContigSize}" "${aveAsmSize}"
LC_NUMERIC=en_US /usr/bin/printf "%s\t%s\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\n" "${dateStamp}" "${groupName}" $orgCount $asmCount $totalContigs $totalSize "${aveContigSize}" "${aveAsmSize}" >> logs/assembly.counts.txt

done

aveContigSize=$(( sumTotalSize/sumContigCount ))
aveAsmSize=$(( sumTotalSize/sumAsmCount ))

LC_NUMERIC=en_US /usr/bin/printf "<tr><td align=right>totals:</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
</tr>" $sumOrgCount $sumAsmCount $sumContigCount $sumTotalSize "${aveContigSize}" "${aveAsmSize}"
LC_NUMERIC=en_US /usr/bin/printf "%s\t%s\t%'d\t%'d\t%'d\t%'d\t%'d\t%'d\n" "${dateStamp}" "totals" $sumOrgCount $sumAsmCount $sumContigCount $sumTotalSize "${aveContigSize}" "${aveAsmSize}" >> logs/assembly.counts.txt

printf "%s\n" \
'</table>
<p>
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
