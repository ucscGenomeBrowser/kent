#!/bin/bash

# the set -x will echo all commands as they execute, for debugging
# set -x

function usage() {
  printf "usage: ncbiPartitionedHtml.sh <viral|bacteria>\n" 1>&2
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

cd /hive/data/inside/ncbi/genomes/refseq/hubs/${subType}

declare -A pageTitles

pageTitles=( ["bacteria"]="Bacterial" \
  ["viral"]="Viral" )

export pageTitle="${pageTitles["$subType"]}"

export inside="/hive/data/inside/ncbi/genomes/refseq"
export groupName="${subType}"

for N in ??
do

htmlFile="${N}/${groupName}.${N}.ncbi.html"

hubLink="refseq/${subType}"

rm -f "${htmlFile}"

printf "<html><head>
    <meta http-equiv=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">
    <title>%s assembly hub</title>
    <link rel=\"STYLESHEET\" href=\"/style/HGStyle.css\">
</head><body>
<!--#include virtual=\"/cgi-bin/hgMenubar\"-->
<h1>%s assembly hub</h1>
<p>
<!--#include virtual=\"../../hubIndex.ncbi.html\"-->
</p>\n" "${pageTitle}" "${pageTitle}" > "${htmlFile}"

orgCount=`cut -f4 ${N}/${groupName}.${N}.order.tab | sort -u | wc -l`
asmCount=`cut -f6 ${N}/${groupName}.${N}.order.tab | sort -u | wc -l`
totalContigs=`cut -f1 ${N}/${groupName}.${N}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
totalSize=`cut -f2 ${N}/${groupName}.${N}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`

printf "<table border=1>\n" >> "${htmlFile}"
tableHeader >> "${htmlFile}"

# every number of lines, print the table header row
export headerFrequency=15
export lineCount=0

sort -t'	' -k6,6 -u ${N}/${groupName}.${N}.order.tab \
  |sort  -t'	' -k4,4f -k3,3nr -k2,2Mr -k1,1nr | cut -f7 | while read F
do
  dirName=`dirname ${F}`
  srcDir="${inside}/${dirName}"
  B=`basename ${F} | sed -e 's/.checkAgpStatusOK.txt//;'`
  /hive/data/inside/ncbi/genomes/refseq/hubs/hubHtml.pl \
      "${hubLink}/${N}" "${srcDir}/${B}" >> "${htmlFile}"
  printf "%s %s %s %s\n" ${groupName} ${hubLink} ${B} ${srcDir}/${B} 1>&2
  lineCount=$(( lineCount+=1 ))
  if [ 0 -eq $(( $lineCount % $headerFrequency )) ]; then
     tableHeader >> "${htmlFile}"
  fi
done

tableHeader >> "${htmlFile}"
LC_NUMERIC=en_US /usr/bin/printf \
"<tr><th align=center colspan=3>total species:&nbsp;%'d</th>
  <th align=center colspan=3>total assemblies:&nbsp;%'d</th>
  <th align=center colspan=3>total contigs:&nbsp;%'d</th>
  <th align=center colspan=4>total nucleotides:&nbsp;%'d</th></tr>
</table>
</body></html>\n" "$orgCount" "$asmCount" "$totalContigs" "$totalSize" \
    >> "${htmlFile}"

chmod +x "${htmlFile}"

done
