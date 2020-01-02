#!/usr/bin/env perl

use strict;
use warnings;

my $Name = "GlobalReference";
my $asmHubWorkDir = "globalReference";
my $defaultAssembly = "GCA_001524155.4_NA19240_prelim_3.0";

my @orderList;	# asmId of the assemblies in order from the *.list files
# the order to read the different .list files:
my @classList = qw( human );
my %class;	# key is asmId, value is from class list
my $assemblyCount = 0;

my %coriellLink;	# key is asmId, value is isolate ID to construct link
# https://www.coriell.org/0/Sections/Search/Sample_Detail.aspx?Ref=<isolateId>
my %ethnicGroup;	# ksy is asmId, value is ethnicity
my %countryOfOrigin;	# ksy is asmId, value is country of origin

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
<!--#set var="TITLE" value="Global Reference Genomes assembly hubs" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>Global Reference Genomes assembly hubs</h1>
<p>
This assembly hub contains ten high-quality, population-identified assemblies released
by the <a href='https://www.genome.wustl.edu/items/reference-genome-improvement/' target=_blank>
Reference Genome Improvement</a> project. This population list is not conclusive and 
represents reference genomes from 10 different human populations. Thank you to 
Washington University in Saint Louis for consolidating these open source reference genomes.
</p>

<h3>How to view the hub</h3>
<p>
You can load this hub from our
<a href="https://genome.ucsc.edu/cgi-bin/hgHubConnect#publicHubs" target="_blank">Public Hubs</a> 
page or by clicking these assembly links to any of our official websites:
<ul>
  <li>
    <a href="https://genome.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/$asmHubWorkDir/hub.txt&amp;genome=$defaultAssembly"
    target="_blank">genome.ucsc.edu</a></li>
  <li> 
    <a href="https://genome-euro.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/$asmHubWorkDir/hub.txt&amp;genome=$defaultAssembly"
    target="_blank">genome-euro.ucsc.edu</a></li>
  <li>
    <a href="https://genome-asia.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/$asmHubWorkDir/hub.txt&amp;genome=$defaultAssembly"
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
    <br><code>https://hgdownload.soe.ucsc.edu/hubs/$asmHubWorkDir/hub.txt</code></li>
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
}

##############################################################################
### start the table output
##############################################################################
sub startTable() {
print <<"END"
<table class="sortable" border="1">
<thead><tr><th>ethnicity<br>link&nbsp;to&nbsp;genome&nbsp;browser</th>
  <th>country&nbsp;of&nbsp;origin<br>and&nbsp;data&nbsp;download</th>
  <th>NCBI&nbsp;assembly</th>
  <th>bioSample</th><th>bioProject</th>
  <th>assembly&nbsp;date,<br>source&nbsp;link</th>
</tr></thead><tbody>
END
}

##############################################################################
### end the table output
##############################################################################
sub endTable() {

print <<"END"

</tbody>
</table>
END
}

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
}

##############################################################################
### tableContents()
##############################################################################
sub tableContents() {
  open (CN, "|sort --ignore-case >commonNameOrder.list") or die "can not write to commonNameOrder.list";

  foreach my $asmId (@orderList) {
#    next if ($asmId =~ m/GCF_900963305.1_fEcheNa1.1/);
    my $asmReport="${asmId}/download/${asmId}_assembly_report.txt";
    my ($gcPrefix, $asmAcc, $asmName) = split('_', $asmId, 3);
    my $chromSizes="${asmId}/${asmId}.chrom.sizes";
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
    printf CN "%s\t%s\n", $commonName, $asmId;
    printf "<tr><td align=center><a href='https://genome.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/%s/hub.txt&amp;genome=%s&amp;position=lastDbPos' target=_blank>%s</a></td>\n", $asmHubWorkDir, $asmId, $ethnicGroup{$asmId};
    printf "    <td align=center><a href='https://hgdownload.soe.ucsc.edu/hubs/%s/genomes/%s/' target=_blank>%s</a></td>\n", $asmHubWorkDir, $asmId, $countryOfOrigin{$asmId};
    printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/assembly/%s_%s/' target=_blank>%s</a></td>\n", $gcPrefix, $asmAcc, $asmId;
    printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/biosample/?term=%s' target=_blank>%s</a></td>\n", $bioSample, $bioSample;
    printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/bioproject/?term=%s' target=_blank>%s</a></td>\n", $bioProject, $bioProject;
    printf "    <td align=center><a href='https://www.coriell.org/0/Sections/Search/Sample_Detail.aspx?Ref=%s' target=_blank>%s</a></td>\n", $coriellLink{$asmId}, $asmDate;
    printf "</tr>\n";
  }
  close(CN);
}

##############################################################################
### main()
##############################################################################

my $home = $ENV{'HOME'};
my $srcDir = "$home/kent/src/hg/makeDb/doc/$asmHubWorkDir";

open (FH, "<$srcDir/isolate.txt") or die "can not read $srcDir/isolate.txt";
while (my $line = <FH>) {
  chomp $line;
  my ($asmId, $isolate) = split('\t', $line);
  $coriellLink{$asmId} = $isolate;
}
close (FH);

open (FH, "<$srcDir/ethnicGroup.txt") or die "can not read $srcDir/ethnicGroup.txt";
while (my $line = <FH>) {
  chomp $line;
  my ($asmId, $ethnicGroup) = split('\t', $line);
  my ($ethnic, $origin) = split(', ', $ethnicGroup);
  $ethnicGroup{$asmId} = $ethnic;
  $countryOfOrigin{$asmId} = $origin;
}
close (FH);

foreach my $species (@classList) {
  my $listFile = "$srcDir/${species}.list";
  open (FH, "<$listFile") or die "can not read $listFile";
  while (my $asmId = <FH>) {
    chomp $asmId;
    push @orderList, $asmId;
    $class{$asmId} = $species;
    ++$assemblyCount;
  }
  close (FH);
}

startHtml();
startTable();
tableContents();
endTable();
endHtml();
