#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 3) {
  printf STDERR "mkAsmStats Name asmName\n";
  printf STDERR "e.g.: mkHubIndex Primates primates GCF_000001405.39_GRCh38.p13\n";
  exit 255;
}
my $Name = shift;
my $asmHubName = shift;
my $defaultAssembly = shift;

my $home = $ENV{'HOME'};
my $toolsDir = "$home/kent/src/hg/makeDb/doc/asmHubs";
my $commonNameOrder = "$asmHubName.commonName.asmId.orderList.tsv";

my @orderList;	# asmId of the assemblies in order from the *.list files
# the order to read the different .list files:
my $assemblyCount = 0;
my %betterName;	# key is asmId, value is a common name better than found
			# in assembly_report file

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

my $subSetMessage = "subset of $asmHubName only";
if ($asmHubName eq "vertebrate") {
   $subSetMessage = "subset of other ${asmHubName}s only";
}

print <<"END"
<!DOCTYPE HTML 4.01 Transitional>
<!--#set var="TITLE" value="$Name genomes assembly hubs" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>$Name Genomes assembly hubs</h1>
<p>
Assemblies from NCBI/Genbank/Refseq sources, $subSetMessage.
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
To manually attach all the assemblies in this hub to other genome browsers:
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
genome assemblies can be selected from the
<em>${Name} Hub Assembly</em> dropdown menu.
Instead of adding all the assemblies in one collected group, use the individual
<em>link to genome browser</em> in the table below.
</p>
<h3>See also: <a href='asmStats${Name}.html'>assembly statistics</a>,&nbsp;<a href='trackData.html'>track statistics</a></h3><br>
<h3>Data resource links</h3>
NOTE: <em>Click on the column headers to sort the table by that column</em><br>
The <em>link to genome browser</em> will attach only that single assembly to
the genome browser.
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

if ($asmHubName ne "viral") {
  printf "<p>\n<table border='1'><thead>\n<tr>";
  printf "<th>Assembly hubs index pages:&nbsp;</th>\n";
  printf "<th><a href='../primates/index.html'>Primates</a></th>\n";
  printf "<th><a href='../mammals/index.html'>Mammals</a></th>\n";
  printf "<th><a href='../birds/index.html'>Birds</a></th>\n";
  printf "<th><a href='../fish/index.html'>Fish</a></th>\n";
  printf "<th><a href='../vertebrate/index.html'>other vertebrates</a></th>\n";

  printf "</tr><tr>\n";
  printf "<th>Hubs assembly statistics:&nbsp;</th>\n";
  printf "<th><a href='../primates/asmStatsPrimates.html'>Primates</a></th>\n";
  printf "<th><a href='../mammals/asmStatsMammals.html'>Mammals</a></th>\n";
  printf "<th><a href='../birds/asmStatsBirds.html'>Birds</a></th>\n";
  printf "<th><a href='../fish/asmStatsFish.html'>Fish</a></th>\n";
  printf "<th><a href='../vertebrate/asmStatsVertebrate.html'>other vertebrates</a></th>\n";

  printf "</tr><tr>\n";
  printf "<th>Hubs track statistics:&nbsp;</th>\n";
  printf "<th><a href='../primates/trackData.html'>Primates</a></th>\n";
  printf "<th><a href='../mammals/trackData.html'>Mammals</a></th>\n";
  printf "<th><a href='../birds/trackData.html'>Birds</a></th>\n";
  printf "<th><a href='../fish/trackData.html'>Fish</a></th>\n";
  printf "<th><a href='../vertebrate/trackData.html'>other vertebrates</a></th>\n";

  printf "</tr></thead>\n</table>\n</p>\n";
}

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
  foreach my $asmId (reverse(@orderList)) {
    my ($gcPrefix, $asmAcc, $asmName) = split('_', $asmId, 3);
    my $accessionId = sprintf("%s_%s", $gcPrefix, $asmAcc);
    my $accessionDir = substr($asmId, 0 ,3);
    $accessionDir .= "/" . substr($asmId, 4 ,3);
    $accessionDir .= "/" . substr($asmId, 7 ,3);
    $accessionDir .= "/" . substr($asmId, 10 ,3);
    my $ncbiFtpLink = "ftp://ftp.ncbi.nlm.nih.gov/genomes/all/$accessionDir/$asmId";
    my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir/$asmId";
    my $asmReport="$buildDir/download/${asmId}_assembly_report.txt";
    my $trackDb="$buildDir/${asmId}.trackDb.txt";
    next if (! -s "$trackDb");
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
           $commonName = $betterName{$asmId} if (exists($betterName{$asmId}));
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
    my $hubUrl = "https://hgdownload.soe.ucsc.edu/hubs/$accessionDir/$accessionId";
    printf "<tr><td align=right>%d</td>\n", ++$rowCount;
###    printf "<td align=center><a href='https://genome.ucsc.edu/cgi-bin/hgGateway?hubUrl=%s/hub.txt&amp;genome=%s&amp;position=lastDbPos' target=_blank>%s</a></td>\n", $hubUrl, $accessionId, $commonName;
    printf "<td align=center><a href='https://genome.ucsc.edu/h/%s' target=_blank>%s</a></td>\n", $accessionId, $commonName;
    printf "    <td align=center><a href='%s/' target=_blank>%s</a></td>\n", $hubUrl, $sciName;
    printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/assembly/%s_%s/' target=_blank>%s</a></td>\n", $gcPrefix, $asmAcc, $asmId;
    if ( $bioSample ne "notFound" ) {
    printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/biosample/?term=%s' target=_blank>%s</a></td>\n", $bioSample, $bioSample;
    } else {
    printf "    <td align=left>n/a</td>\n";
    }
    printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/bioproject/?term=%s' target=_blank>%s</a></td>\n", $bioProject, $bioProject;
    printf "    <td align=center><a href='%s' target=_blank>%s</a></td>\n", $ncbiFtpLink, $asmDate;
    printf "</tr>\n";
  }
}	#	sub tableContents()

##############################################################################
### main()
##############################################################################

open (FH, "<$toolsDir/${commonNameOrder}") or die "can not read ${commonNameOrder}";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my ($commonName, $asmId) = split('\t', $line);
  push @orderList, $asmId;
  $betterName{$asmId} = $commonName;
  ++$assemblyCount;
}
close (FH);

startHtml();
startTable();
tableContents();
endTable();
endHtml();
