#!/bin/bash

set -beEu -o pipefail

export srcDir="$HOME/kent/src/hg/gar"

rsync -aP hgdownload.soe.ucsc.edu::hubs/UCSC_GI.assemblyHubList.txt ./ \
  > /dev/null 2>&1

### starting html page output ##############################################
printf "<!DOCTYPE HTML 4.01 Transitional>
<!--#set var='TITLE' value='Genome assembly search and request' -->
<!--#set var='ROOT' value='.' -->

<!--#include virtual='\$ROOT/inc/gbPageStartHardcoded.html' -->

<link rel='stylesheet' type='text/css' href='<!--#echo var='ROOT' -->/style/gar.css'>

"

# coordinate these colors with the table.pl definitions
export criticalColor="#ff0000"
export endangeredColor="#dd6600"
export vulnerableColor="#663300"

export leftHandPointer="&#9756;"
export rightHandPointer="&#9758;"

# printf "<a id='pageTop'></a>\n"

printf "<h2>Genome assembly search and request</h2>\n"

printf "<div id='canNotFindDiv' class='pullDownMenu'>\n"
printf "  <span id='canNotFindAnchor'>after searching page,<br>cannot find desired assembly here</span'>\n"
printf "  <div class='pullDownMenuContent'>\n"
printf "   <button id='specificRequest' type='button' onclick='gar.openModal(this)' name='specific'><label>%s request specific assembly not found here %s</label></button>\n", "${rightHandPointer}" "${leftHandPointer}" 
printf "  </div>\n"
printf "</div>\n"

printf "<p>Please note discussion of <a href='http://genome.ucsc.edu/goldenPath/help/hgTracksHelp.html#What' target=_blank>What does the Genome Browser do ?</a>\n"
printf "</p>\n"

printf "<p>The information in this page allows navigation to
specific genome browsers when they are available.  In the case where a
genome browser has not yet been constructed, the link for that species will
open a request form which will notify the U.C. Genome Browser staff as a request
to construct the genome browser for your species of interest.  This is a new
and experimental service, we are interested to see if such requests are
many or few.  Depending upon the level of activity, the expected processing
time for an assembly to appear in the genome browser should be on the order of
two weeks.
</p>\n"

printf "<p>Please note, not all possible assemblies are shown here.  There is
nothing here from the clade categories: <em>archaea, viruses, bacteria</em>.
This display attempts to show only whole genome assemblies.  Some assemblies
have been eliminated when their total assembly size is much smaller than the
typical genome size for such species.  For example, only the <em>exome</em>
of the organism has been sequenced.  And in some cases there are too many
assemblies for the same species, for example there are over 1,000 assemblies for <em>Homo sapiens</em>.
</p>\n"

printf "<p>This page works best in the <em>Firefox</em> and <em>Safari</em>
web browsers.  The web browsers <em>Chrome</em> and <em>Edge</em> take
almost a minute to prepare the page for use.
</p>\n"

printf "<p>Use your web browser <em>find in page</em> function to search
for names of interest on this page.  Make the entire set visiible to allow
the search to find everything.  Future enhancements to this page may include
a better search system.
</p>\n"

# printf "<p>The numbers in parenthesis following the
# <em>Scientific name (1,234)</em> indicate the number of assemblies available
# for that species.
# </p>\n"

# my @clades = qw( primates mammals birds fish vertebrate invertebrates plants fungi );

cut -d' ' -f3,5 \
  /cluster/home/hiram/kent/src/hg/makeDb/doc/asmHubs/master.run.list \
    | sort | awk '{printf "%s\t%s\n", $1, $2};' > asmId.sciName

sort -k1,1 -u \
  /cluster/home/hiram/kent/src/hg/makeDb/doc/*AsmHub/*.orderList.tsv \
    > asmId.commonName;

sort -u /hive/data/outside/ncbi/genomes/reports/newAsm/*.suppressed.asmId.list \
  > asmId.suppressed.list 

join -t$'\t' asmId.sciName asmId.commonName > asmId.sciName.commonName

$srcDir/garTable.pl

# add in the modal popUp request hidden window
printf "<!--#include virtual='\$ROOT/inc/garModal.html' -->
</div><!-- closing gbsPage from gbPageStartHardcoded.html -->
</div><!-- closing container-fluid from gbPageStartHardcoded.html -->
<!--#include virtual='\$ROOT/inc/gbFooterHardcoded.html' -->
<script src='<!--#echo var='ROOT' -->/js/sorttable.js'></script>
<script src='<!--#echo var='ROOT' -->/js/gar.js'></script>
</body></html>\n"
