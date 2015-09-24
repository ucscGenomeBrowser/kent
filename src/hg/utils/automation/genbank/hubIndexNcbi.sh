#!/bin/bash

# set -x
cd /hive/data/inside/ncbi/genomes/genbank/hubs

declare -A hubLinks

hubLinks=( ["plant"]="gbkPlant" ["fungi"]="gbkFungi" ["other"]="gbkOther" \
  ["archaea"]="gbkArchaea" ["protozoa"]="gbkProtozoa" \
  ["vertebrate_mammalian"]="gbkVertebrateMammalian" \
  ["vertebrate_other"]="gbkVertebrateOther" \
  ["bacteria"]="gbkBacteria" \
  ["invertebrate"]="gbkInvertebrate" )

export inside="/hive/data/inside/ncbi/genomes/genbank"

printf "%s\n" \
'<p>Collection of hubs for genbank genome assemblies.
Use these links to go to the index for that subset of assemblies:
<table border=1><tr>
  <th>species<br>subset</th>
  <th>number&nbsp;of<br>species</th>
  <th>number&nbsp;of<br>assemblies</th>
  <th>total&nbsp;contig<br>count</th>
  <th>total&nbsp;nucleotide<br>count</th>
</tr>'

export sumOrgCount=0
export sumAsmCount=0
export sumContigCount=0
export sumTotalSize=0

for groupName in archaea bacteria fungi invertebrate plant protozoa other vertebrate_mammalian vertebrate_other
do

# hubLink="${hubLinks["$groupName"]}"
hubLink="genbank/${groupName}"

orgCount=`cut -f4 ${groupName}.order.tab | sort -u | wc -l`
asmCount=`cut -f6 ${groupName}.order.tab | sort -u | wc -l`
totalContigs=`cut -f1 ${groupName}/${groupName}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
totalSize=`cut -f2 ${groupName}/${groupName}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
sumOrgCount=$(( sumOrgCount+orgCount ))
sumAsmCount=$(( sumAsmCount+asmCount ))
sumContigCount=$(( sumContigCount+totalContigs ))
sumTotalSize=$(( sumTotalSize+totalSize ))
export groupLabel="${groupName}"
if [ "${groupName}" = "other" ]; then
   groupLabel="${groupName}/synthetic assemblies"
fi

LC_NUMERIC=en_US /usr/bin/printf "<tr><td align=right><a href=\"http://genome-test.cse.ucsc.edu/~hiram/hubs/%s/%s.ncbi.html\">%s</a></td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
</tr>" $hubLink $groupName "$groupLabel" $orgCount $asmCount $totalContigs $totalSize

done

LC_NUMERIC=en_US /usr/bin/printf "<tr><td align=right>totals:</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
  <td align=right>%'d</td>
</tr>" $sumOrgCount $sumAsmCount $sumContigCount $sumTotalSize

printf "%s\n" \
'</table>
<p>
<b>NOTE:</b> This is a prototype work in progress.  Not all assemblies are
represented here yet.  There are no gene tracks yet on these genome browsers,
they will need a gene track to begin to become useful.  This system may
not be available at all times as the procedures are finalized of how
to keep this up to date on an automatic process.
</p>
<p>
The "Taxon ID" link will go to the Entrez taxonomy for that ID.<br>
The "common name" link will go to the UCSC genome browser for that assembly.<br>
The "biosample" link will go to the Entrez biosample for that ID.<br>
The "accession" link will go to the Entrez assembly for that ID.<br>
The "assembly" link will go to the NCBI Genbank FTP source directory.
</p>'
