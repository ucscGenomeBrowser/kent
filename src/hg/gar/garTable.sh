#!/bin/bash

set -beEu -o pipefail

export srcDir="$HOME/kent/src/hg/gar"

rsync -aP qateam@hgdownload.soe.ucsc.edu:/mirrordata/hubs/UCSC_GI.assemblyHubList.txt ./ \
  > /dev/null 2>&1

### starting html page output ##############################################
printf "<!DOCTYPE HTML 4.01 Transitional>
<!--#set var='TITLE' value='Genome assembly search and request' -->
<!--#set var='ROOT' value='.' -->

<!--#include virtual='\$ROOT/inc/gbPageStartHardcoded.html' -->

<link rel='stylesheet' type='text/css' href='<!--#echo var='ROOT' -->/style/gar.css'>

"

# coordinate these colors with the table.pl definitions
# export criticalColor="#ff0000"
# export endangeredColor="#dd6600"
# export vulnerableColor="#663300"
export criticalColor="#ee3333";
export endangeredColor="#333388";
export vulnerableColor="#88aaaa";

export leftHandPointer="&#9756;"
export rightHandPointer="&#9758;"

# printf "<a id='pageTop'></a>\n"

printf "<h1>Genome assembly search and request</h1>\n"
printf "<h2>What is the Genome Browser?</h2>\n"

printf "<div id='canNotFindDiv' class='pullDownMenu'>\n"
printf "  <span id='canNotFindAnchor'>Can't find your assembly? &#9660;</span>\n"
printf "  <div class='pullDownMenuContent'>\n"
printf "   <label><button id='specificRequest' type='button' onclick='gar.openModal(this)' name='specific'>%s Press here to request an unlisted assembly %s</button></label>\n", "${rightHandPointer}" "${leftHandPointer}"
printf "  </div>\n"
printf "</div>\n"

printf "<p>The UCSC Genome Browser provides a rapid and reliable display of any
requested portion of any genome assembly at any scale, together with dozens
of aligned annotation tracks (genes, mRNAs, CpG islands, regulation,
variation, repeats, and more). The Genome Browser stacks annotation tracks
beneath genome coordinate positions, allowing rapid visual correlation of
different types of information. The user can look at a whole chromosome to
get a feel for gene density, open a specific cytogenetic band to see a
positionally mapped disease gene candidate, or zoom in to a particular gene
to view its spliced ESTs and possible alternative splicing. The Genome Browser
itself does not draw conclusions; rather, it collates all relevant information
in one location, leaving the exploration and interpretation to the user. For
more information on using the UCSC Genome Browser please see our
<a href='https://genome.ucsc.edu/FAQ/' target=_blank>help</a> and
<a href='https://genome.ucsc.edu/training/index.html' target=_blank>training</a>
pages.
</p>

<h2>What is this page for?</h2>
<p>This page lists both whole-genome assembly browsers that are available for
immediate viewing, and assemblies that are not currently available but can
be requested.
</p>
<p>We are working on adding a search function to this page.  To search this page:
<button onclick='gar.showAll(true)'>Click this to turn on all items for search.</button>
Use the &quot;find&quot; feature of your Browser (commonly CTRL+F).
</p>
<p>After searching the page, if you do not find the assembly you are
interested in, you may request it using the <em>&quot;Can't find your assembly?&quot;</em>
button at the top of the page. Complete the request form (including the
NCBI GenBank or RefSeq assembly accession identifier). You will be notified
by email when your assembly is available for viewing in the UCSC Genome
Browser. This can take up to three weeks. The assembly browser will include
the following annotation tracks: Assembly mapping, Base position,
Gaps, GC Percent, Restriction Enzymes, Tandem Duplications,
CpG Islands, RefSeq mRNAs, Repeat Masking, and, if available:
NCBI RefSeq Genes and Ensembl Genes.
</p>
<p>This page is optimized for use in Firefox and Safari.
</p>
<p>For more details on using this page, please see this <a href='https://genome-blog.soe.ucsc.edu/blog/2022/04/12/genark-hubs-part-4/' target=_blank>blog post</a>.
</p>\n"

grep -v "^#" \
  /cluster/home/hiram/kent/src/hg/makeDb/doc/asmHubs/master.run.list \
    | cut -d' ' -f2,4 | awk '{printf "%s\t%s\n", $1, $2};' | sort -k1,1 -u \
       > asmId.sciName

ls /cluster/home/hiram/kent/src/hg/makeDb/doc/*AsmHub/*.orderList.tsv \
  | grep -v others.orderList.tsv | xargs grep -h -v "^#" | sort -k1,1 -u \
    > asmId.commonName;

sort -u /hive/data/outside/ncbi/genomes/reports/newAsm/*.suppressed.asmId.list \
  > asmId.suppressed.list

join -t$'\t' asmId.sciName asmId.commonName > asmId.sciName.commonName

$srcDir/garTable.pl

# the column 1 tooltip display window, controlled in javaScript
printf "<div id='col1ToolTip' class='col1ToolTip'>request</div>\n"

# add in the modal popUp request hidden window
printf "<!--#include virtual='\$ROOT/inc/garModal.html' -->
</div><!-- closing gbsPage from gbPageStartHardcoded.html -->
</div><!-- closing container-fluid from gbPageStartHardcoded.html -->
<!--#include virtual='\$ROOT/inc/gbFooterHardcoded.html' -->
<script src='<!--#echo var='ROOT' -->/js/sorttable.js'></script>
<script src='<!--#echo var='ROOT' -->/js/gar.js'></script>
</body></html>\n"
