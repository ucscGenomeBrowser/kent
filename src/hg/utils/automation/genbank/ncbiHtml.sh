#!/bin/bash

# the set -x will echo all commands as they execute, for debuggin
# set -x

function tableHeader() {
printf "<tr><th>Taxon ID</th>
  <th>date</th><th>common<br>name</th><th>scientific<br>name</th>
  <th>biosample</th><th>contig<br>count</th><th>genome<br>size</th>
  <th>N50&nbsp;size</th><th>GC&nbsp;percent</th>
  <th>unknown&nbsp;bases<br>count/percent</th>
  <th>gene&nbsp;count<br>bases<br>percent</th>
  <th>accession</th><th>assembly<br>name</th><th>assembly<br>type</th>
  <th>assembly<br>level</th><th>submitter</th></tr>\n"
}

cd /hive/data/inside/ncbi/genomes/refseq/hubs

declare -A pageTitles

pageTitles=( ["plant"]="Plant assembly hub" \
  ["fungi"]="Fungi assembly hub" \
  ["other"]="Other/Synthetic genomic assembly hub" \
  ["archaea"]="Archaea assembly hub" \
  ["protozoa"]="Protozoa assembly hub" \
  ["vertebrate_mammalian"]="Vertebrate Mammalian assembly hub" \
  ["vertebrate_other"]="non-Mammalian other Vertebrate assembly hub" \
  ["invertebrate"]="Invertebrates assembly hub" )

export inside="/hive/data/inside/ncbi/genomes/refseq"

# archaea
# fungi
# invertebrate
# plant
# protozoa
# vertebrate_mammalian
# vertebrate_other

# for groupName in archaea fungi invertebrate other plant protozoa vertebrate_mammalian vertebrate_other

# for groupName in archaea fungi invertebrate plant protozoa vertebrate_mammalian vertebrate_other
for groupName in plant protozoa invertebrate vertebrate_other vertebrate_mammalian fungi archaea
do

hubLink="refseq/${groupName}"
pageTitle="${pageTitles["$groupName"]}"

htmlFile="${groupName}/${groupName}.ncbi.html"
rm -f "${htmlFile}"

printf "<html><head>
    <meta http-equiv=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">
    <title>%s</title>
    <link rel=\"STYLESHEET\" href=\"/style/HGStyle.css\">
</head><body>
<!--#include virtual=\"/cgi-bin/hgMenubar\"-->
<h1>%s</h1>
<p>
<!--#include virtual=\"../hubIndex.ncbi.html\"-->
</p>\n" "${pageTitle}" "${pageTitle}" > "${htmlFile}"

orgCount=`cut -f4 ${groupName}.order.tab | sort -u | wc -l`
asmCount=`cut -f6 ${groupName}.order.tab | sort -u | wc -l`
totalContigs=`cut -f1 ${groupName}/${groupName}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
totalSize=`cut -f2 ${groupName}/${groupName}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`

printf "<table border=1>\n" >> "${htmlFile}"
tableHeader >> "${htmlFile}"

# every number of lines, print the table header row
export headerFrequency=15
export lineCount=0
export lastHeader=0

sort -t'	' -k6,6 -u ${groupName}.order.tab \
  |sort  -t'	' -k4,4f -k3,3nr -k2,2Mr -k1,1nr | cut -f7 | sed -e '/^$/d;' \
| while read F
do
  dirName=`dirname ${F}`
  srcDir="${inside}/${dirName}"
  B=`basename ${F} | sed -e 's/.checkAgpStatusOK.txt//;'`
  ./hubHtml.pl "${hubLink}" "${srcDir}/${B}" >> "${htmlFile}"
  printf "%s %s %s %s\n" ${groupName} ${hubLink} ${B} ${srcDir}/${B} 1>&2
  lineCount=$(( lineCount+=1 ))
  lastHeader=$(( lastHeader+=1 ))
  if [ 0 -eq $(( $lineCount % $headerFrequency )) ]; then
     tableHeader >> "${htmlFile}"
     lastHeader=0
  fi
done

if [ $lastHeader -gt 10 ]; then
 tableHeader >> "${htmlFile}"
fi
LC_NUMERIC=en_US /usr/bin/printf \
"<tr><th align=center colspan=3>total species:&nbsp;%'d</th>
  <th align=center colspan=3>total assemblies:&nbsp;%'d</th>
  <th align=center colspan=3>total contigs:&nbsp;%'d</th>
  <th align=center colspan=4>total nucleotides:&nbsp;%'d</th></tr>
</table>
</body></html>\n" $orgCount $asmCount $totalContigs $totalSize \
    >> "${htmlFile}"
chmod +x "${htmlFile}"

done
