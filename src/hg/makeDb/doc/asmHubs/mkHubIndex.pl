#!/usr/bin/env perl
#
# mkHubIndex.pl - construct index.html page for a set of assemblies in a hub
#

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin";
use commonHtml;

my $argc = scalar(@ARGV);
if ($argc != 4) {
  printf STDERR "mkHubIndex.pl Name asmName defaultAsmId [two column name list] > index.html\n";
  printf STDERR "e.g.: mkHubIndex Primates primates GCF_000001405.39_GRCh38.p13 primates.commonName.asmId.orderList.tsv\n";
  printf STDERR "the name list is found in \$HOME/kent/src/hg/makeDb/doc/asmHubs/\n";
  printf STDERR "\nthe two columns are 1: asmId (accessionId_assemblyName)\n";
  printf STDERR "column 2: common name for species, columns separated by tab\n";
  printf STDERR "The result prints to stdout the index.html page for this set of assemblies\n";
  exit 255;
}

my $home = $ENV{'HOME'};
my $toolsDir = "$home/kent/src/hg/makeDb/doc/asmHubs";

my $Name = shift;
my $asmHubName = shift;
my $defaultAssembly = shift;
my $inputList = shift;
my $orderList = $inputList;
if ( ! -s "$orderList" ) {
  $orderList = $toolsDir/$inputList;
}

printf STDERR "# mkHubIndex %s %s %s %s\n", $Name, $asmHubName, $defaultAssembly, $orderList;
my $vgpIndex = 0;
$vgpIndex = 1 if ($Name =~ m/vgp/i);
my %vgpClass;	# key is asmId, value is taxon 'class' as set by VGP project
if ($vgpIndex) {
  my $vgpClass = "$home/kent/src/hg/makeDb/doc/vgpAsmHub/vgp.taxId.asmId.class.txt";
  open (FH, "<$vgpClass") or die "can not read $vgpClass";
  while (my $line = <FH>) {
    my ($taxId, $asmId, $class) = split('\t', $line);
    $vgpClass{$asmId} = $class;
  }
  close (FH);
}

my @orderList;	# asmId of the assemblies in order from the *.list files
# the order to read the different .list files:
my $assemblyCount = 0;
my %commonName;	# key is asmId, value is a common name, perhaps more appropriate
                # than found in assembly_report file

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

if ($vgpIndex) {
  my $vgpSubset = "(set of primary assemblies)";
  if ($orderList =~ m/vgp.alternate/) {
     $vgpSubset = "(set of alternate/haplotype assemblies)";
  } elsif ($orderList =~ m/vgp.trio/) {
     $vgpSubset = "(set of trio assemblies, maternal/paternal)";
  } elsif ($orderList =~ m/vgp.legacy/) {
     $vgpSubset = "(set of legacy/superseded assemblies)";
  }
  print <<"END"
<!DOCTYPE HTML 4.01 Transitional>
<!--#set var="TITLE" value="VGP - Vertebrate Genomes Project assembly hub" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>VGP - Vertebrate Genomes Project assembly hub</h1>
<p>
<a href='https://vertebrategenomesproject.org/' target=_blank>
<img src='VGPlogo.png' width=280 alt='VGP logo'></a></p>
<p>
This assembly hub contains assemblies released
by the <a href='https://vertebrategenomesproject.org/' target=_blank>
Vertebrate Genomes Project.</a> $vgpSubset
</p>

END
} else {
  print <<"END"
<!DOCTYPE HTML 4.01 Transitional>
<!--#set var="TITLE" value="$Name genomes assembly hubs" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>$Name Genomes assembly hubs</h1>
<p>
Assemblies from NCBI/Genbank/Refseq sources, $subSetMessage.
</p>

END
}

print <<"END"
<h3>How to view the hub</h3>
<p>
Options:
<ol>
  <li>The links to the genome browser in the table below will attach that
      one specific assembly to the genome browser.  This is most likely what
      you want.  Alternatively, the entire set of assemblies can be attached
      as one group to the genome browser with the following links depending
      upon which of our mirror site browsers you prefer to use:
<table border="1">
<tr>
  <th>attach all assemblies to selected site:</th>
  <th>&nbsp;</th>
  <th><a href="https://genome.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/$asmHubName/hub.txt&amp;genome=$defaultAssembly"
        target="_blank">genome.ucsc.edu</a></th>
  <th>&nbsp;</th>
  <th><a href="https://genome-euro.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/$asmHubName/hub.txt&amp;genome=$defaultAssembly"
        target="_blank">genome-euro.ucsc.edu</a></th>
  <th>&nbsp;</th>
  <th><a href="https://genome-asia.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/$asmHubName/hub.txt&amp;genome=$defaultAssembly"
        target="_blank">genome-asia.ucsc.edu</a></th>
</tr>
</table>
  </li>
  <li>To manually attach all the assemblies in this hub to genome browsers
      that are not one of the three UCSC mirror sites:
    <ol>
      <li>From the blue navigation bar, go to
    <em><strong>My Data</strong> -&gt; <strong>Track Hubs</strong></em></li>
      <li>Then select the <strong>My Hubs</strong> tab and enter this URL into
          the textbox:
    <br><code>https://hgdownload.soe.ucsc.edu/hubs/$asmHubName/hub.txt</code></li>
      <li> Once you have added the URL to the entry form,
           press the <em><strong>Add Hub</strong></em> button to add the hub.</li>
    </ol>
  </li>
</ol>
</p>

<p>
After adding the hub, you will be redirected to the gateway page.  The
genome assemblies can be selected from the
<em>${Name} Hub Assembly</em> dropdown menu.
Instead of adding all the assemblies in one collected group, use the individual
<em>view in browser</em> in the table below.
</p>
<h3>See also: <a href='asmStats.html'>assembly statistics</a>,&nbsp;<a href='trackData.html'>track statistics</a> <== additional information for these assemblies.</h3><br>
<h3>Data resource links</h3>
NOTE: <em>Click on the column headers to sort the table by that column</em><br>
The <em>common name and view in browser</em> will attach only that single assembly to
the genome browser.<br>
The <em>scientific name and data download</em> link provides access to the files for that one
assembly hub.<br>
The <em>class VGP link</em> provides access to the VGP GenomeArk page for that genome.<br>
The other links provide access to NCBI resources for these assemblies.
END
}	#	sub startHtml()

##############################################################################
### start the table output
##############################################################################
sub startTable() {
print '
<table class="sortable" border="1">
<thead><tr><th>count</th>
  <th>common&nbsp;name&nbsp;and<br>view&nbsp;in&nbsp;browser</th>
  <th>scientific name<br>and&nbsp;data&nbsp;download</th>
  <th>NCBI&nbsp;assembly</th>
  <th>BioSample</th><th>BioProject</th>
  <th>assembly&nbsp;date,<br>source&nbsp;link</th>
';

if ($vgpIndex) {
  printf "<th>class<br>VGP&nbsp;link</th>\n";
}
print "</tr></thead><tbody>\n";
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

&commonHtml::otherHubLinks($vgpIndex, $asmHubName);
&commonHtml::htmlFooter($vgpIndex, $asmHubName);

}	#	sub endHtml()

##############################################################################
### tableContents()
##############################################################################
sub tableContents() {
  my $rowCount = 0;
  foreach my $asmId (@orderList) {
    my ($gcPrefix, $asmAcc, $asmName) = split('_', $asmId, 3);
    my $accessionId = sprintf("%s_%s", $gcPrefix, $asmAcc);
    my $accessionDir = substr($asmId, 0 ,3);
    $accessionDir .= "/" . substr($asmId, 4 ,3);
    $accessionDir .= "/" . substr($asmId, 7 ,3);
    $accessionDir .= "/" . substr($asmId, 10 ,3);
    my $ncbiFtpLink = "ftp://ftp.ncbi.nlm.nih.gov/genomes/all/$accessionDir/$asmId";
    my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir/$asmId";
    if ($gcPrefix eq "GCA") {
     $buildDir = "/hive/data/genomes/asmHubs/genbankBuild/$accessionDir/$asmId";
    }
    my $asmReport="$buildDir/download/${asmId}_assembly_report.txt";
    my $trackDb="$buildDir/${asmId}.trackDb.txt";
    next if (! -s "$trackDb");	# assembly build not complete
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
           $commonName = $commonName{$asmId} if (exists($commonName{$asmId}));
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
    # one broken assembly_report
    $bioProject= "PRJEB25768" if ($accessionId eq "GCA_900324465.2");
    if ($bioProject eq "notFound") {
      printf "    <td align=left>%s</td>\n", $bioProject;
    } else {
      printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/bioproject/?term=%s' target=_blank>%s</a></td>\n", $bioProject, $bioProject;
    }
    printf "    <td align=center><a href='%s' target=_blank>%s</a></td>\n", $ncbiFtpLink, $asmDate;
    if ($vgpIndex) {
      my $sciNameUnderscore = $sciName;
      $sciNameUnderscore =~ s/ /_/g;
      $sciNameUnderscore = "Strigops_habroptilus" if ($sciName =~ m/Strigops habroptila/);

      if (! defined($vgpClass{$asmId})) {
         printf STDERR "# ERROR: no 'class' defined for VGP assembly %s\n", $asmId;
         exit 255;
      }
      printf "    <td align=center><a href='https://vgp.github.io/genomeark/%s/' target=_blank>%s</a></td>\n", $sciNameUnderscore, $vgpClass{$asmId}
    }
    printf "</tr>\n";
  }
}	#	sub tableContents()

##############################################################################
### main()
##############################################################################

open (FH, "<${orderList}") or die "can not read ${orderList}";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my ($asmId, $commonName) = split('\t', $line);
  push @orderList, $asmId;
  $commonName{$asmId} = $commonName;
  ++$assemblyCount;
}
close (FH);

startHtml();
startTable();
tableContents();
endTable();
endHtml();
