#!/usr/bin/env perl

use strict;
use warnings;

my $home = $ENV{'HOME'};
my $srcDocDir = "primateAsmHub";
my $asmHubDocDir = "$home/kent/src/hg/makeDb/doc/$srcDocDir";
my $Name = "Primates";
my $asmHubName = "primates";
my $defaultAssembly = "GCF_000001405.39_GRCh38.p13";

my $srcDir = "$home/kent/src/hg/makeDb/doc/$srcDocDir";
my $commonNameList = "primates.asmId.commonName.tsv";
my $commonNameOrder = "primates.commonName.asmId.orderList.tsv";

my @orderList;	# asmId of the assemblies in order from the *.list files
# the order to read the different .list files:
my @classList = qw( human );
my %class;	# key is asmId, value is from class list
my $assemblyCount = 0;

##############################################################################
# from Perl Cookbook Recipe 2.17, print out large numbers with comma delimiters:
##############################################################################
sub commify($) {
    my $text = reverse $_[0];
    $text =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text
}

##############################################################################
### start the HTML output
##############################################################################
sub startHtml() {

my $timeStamp = `date "+%F"`;
chomp $timeStamp;

# <html xmlns="http://www.w3.org/1999/xhtml">

print <<"END"
<!DOCTYPE HTML 4.01 Transitional>
<!--#set var="TITLE" value="Primate genomes assembly hubs" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>Primate Genomes assembly hubs</h1>
<p>
Assemblies from NCBI/Genbank/Refseq sources
</p>

<h3>How to view the hub</h3>
<p>
You can load this hub from our
<a href="https://genome.ucsc.edu/cgi-bin/hgHubConnect#publicHubs" target="_blank">Public Hubs</a> 
page or by clicking these assembly links to any of our official websites:
<ul>
  <li>
    <a href="https://genome.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/$asmHubName/hub.txt&amp;genome=$defaultAssembly"
    target="_blank">genome.ucsc.edu</a></li>
  <li> 
    <a href="https://genome-euro.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/$asmHubName/hub.txt&amp;genome=$defaultAssembly"
    target="_blank">genome-euro.ucsc.edu</a></li>
  <li>
    <a href="https://genome-asia.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/$asmHubName/hub.txt&amp;genome=$defaultAssembly"
    target="_blank">genome-asia.ucsc.edu</a></li>
</ul>
</p>

<p>
To manually attach this hub to other genome browsers:
<ol>
  <li>
    From the blue navigation bar, go to
    <em><strong>My Data</strong> -&gt; <strong>Track Hubs</strong></em></li>
  <li>
    Then select the <strong>My Hubs</strong> tab and enter this URL into the textbox:
    <br><code>https://hgdownload.soe.ucsc.edu/hubs/$asmHubName/hub.txt</code></li>
  <li>
    Once you have added the URL to the entry form, press the <em><strong>Add Hub</strong></em>
    button to add the hub.</li>
</ol>
</p>

<p>
After adding the hub, you will be redirected to the gateway page.  The
genome assemblies can be selected from the <em>Reference Genome Improvement Hub Assembly</em> dropdown menu.
</p>
<p>
<h3>See also: <a href='asmStats$Name.html' target=_blank>assembly statistics</a></h3>
</p>
<h3>Data resource links</h3>
NOTE: <em>Click on the column headers to sort the table by that column</em>
END
}	#	sub startHtml()

##############################################################################
### start the table output
##############################################################################
sub startTable() {
print <<"END"
<table class="sortable" border="1">
<thead><tr><th>count</th>
  <th>common name<br>link&nbsp;to&nbsp;genome&nbsp;browser</th>
  <th>scientific name<br>and&nbsp;data&nbsp;download</th>
  <th>NCBI&nbsp;assembly</th>
  <th>bioSample</th><th>bioProject</th>
  <th>assembly&nbsp;date,<br>source&nbsp;link</th>
</tr></thead><tbody>
END
}	#	sub startTable()

##############################################################################
### end the table output
##############################################################################
sub endTable() {

print <<"END"

</tbody>
</table>
END
}	#	sub endTable()

##############################################################################
### end the HTML output
##############################################################################
sub endHtml() {
print <<"END"
</div><!-- closing gbsPage from gbPageStartHardcoded.html -->
</div><!-- closing container-fluid from gbPageStartHardcoded.html -->
<!--#include virtual="\$ROOT/inc/gbFooterHardcoded.html"-->
<script type="text/javascript" src="/js/sorttable.js"></script>
</body></html>
END
}	#	sub endHtml()

##############################################################################
### tableContents()
##############################################################################
sub tableContents() {
  my $rowCount = 0;
  foreach my $asmId (@orderList) {
    my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/" . substr($asmId, 0 ,3);
    $buildDir .= "/" . substr($asmId, 4 ,3);
    $buildDir .= "/" . substr($asmId, 7 ,3);
    $buildDir .= "/" . substr($asmId, 10 ,3);
    $buildDir .= "/" . $asmId;
    my $asmReport="$buildDir/download/${asmId}_assembly_report.txt";
    my ($gcPrefix, $asmAcc, $asmName) = split('_', $asmId, 3);
    my $chromSizes="${buildDir}/${asmId}.chrom.sizes";
    my $sciName = "notFound";
    my $commonName = "notFound";
    my $bioSample = "notFound";
    my $bioProject = "notFound";
    my $taxId = "notFound";
    my $asmDate = "notFound";
    my $itemsFound = 0;
    open (FH, "<$asmReport") or die "can not read $asmReport";
    while (my $line = <FH>) {
      last if ($itemsFound > 5);
      chomp $line;
      $line =~ s///g;;
      $line =~ s/\s+$//g;;
      if ($line =~ m/Date:/) {
        if ($asmDate =~ m/notFound/) {
           ++$itemsFound;
           $line =~ s/.*:\s+//;
           my @a = split('-', $line);
           $asmDate = sprintf("%04d-%02d-%02d", $a[0], $a[1], $a[2]);
        }
      } elsif ($line =~ m/BioSample:/) {
        if ($bioSample =~ m/notFound/) {
           ++$itemsFound;
           $bioSample = $line;
           $bioSample =~ s/.*:\s+//;
        }
      } elsif ($line =~ m/BioProject:/) {
        if ($bioProject =~ m/notFound/) {
           ++$itemsFound;
           $bioProject = $line;
           $bioProject =~ s/.*:\s+//;
        }
      } elsif ($line =~ m/Organism name:/) {
        if ($sciName =~ m/notFound/) {
           ++$itemsFound;
           $commonName = $line;
           $sciName = $line;
           $commonName =~ s/.*\(//;
           $commonName =~ s/\)//;
           $sciName =~ s/.*:\s+//;
           $sciName =~ s/\s+\(.*//;
        }
      } elsif ($line =~ m/Taxid:/) {
        if ($taxId =~ m/notFound/) {
           ++$itemsFound;
           $taxId = $line;
           $taxId =~ s/.*:\s+//;
        }
      }
    }
    close (FH);
    printf "<tr><td align=right>%d</td>\n", ++$rowCount;
    printf "<td align=center><a href='https://genome.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/%s/hub.txt&amp;genome=%s&amp;position=lastDbPos' target=_blank>%s</a></td>\n", $asmHubName, $asmId, $commonName;
    printf "    <td align=center><a href='https://hgdownload.soe.ucsc.edu/hubs/%s/genomes/%s/' target=_blank>%s</a></td>\n", $asmHubName, $asmId, $sciName;
    printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/assembly/%s_%s/' target=_blank>%s</a></td>\n", $gcPrefix, $asmAcc, $asmId;
    if ( $bioSample ne "notFound" ) {
    printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/biosample/?term=%s' target=_blank>%s</a></td>\n", $bioSample, $bioSample;
    } else {
    printf "    <td align=left>n/a</td>\n";
    }
    printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/bioproject/?term=%s' target=_blank>%s</a></td>\n", $bioProject, $bioProject;
    printf "    <td align=center>%s</td>\n", $asmDate;
    printf "</tr>\n";
  }
}	#	sub tableContents()

##############################################################################
### main()
##############################################################################

open (FH, "<$srcDir/${commonNameOrder}") or die "can not read ${commonNameOrder}";
while (my $line = <FH>) {
  chomp $line;
  my ($commonName, $asmId) = split('\t', $line);
  push @orderList, $asmId;
  ++$assemblyCount;
}
close (FH);

startHtml();
startTable();
tableContents();
endTable();
endHtml();
