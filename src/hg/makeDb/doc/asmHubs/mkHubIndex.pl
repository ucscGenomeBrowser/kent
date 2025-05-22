#!/usr/bin/env perl
#
# mkHubIndex.pl - construct index.html page for a set of assemblies in a hub
#

use strict;
use warnings;
use File::Basename;
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
my $sciNameOverrideFile = "$toolsDir/sciNameOverride.txt";
my %sciNameOverride;	# key is accession, value is corrected scientific name
my %taxIdOverride;	# key is accession, value is corrected taxId
			# keys for both of those can also be the asmId

if ( -s "${sciNameOverrideFile}" ) {
  open (my $sn, "<", "${sciNameOverrideFile}") or die "can not read ${sciNameOverrideFile}";
  while (my $line = <$sn>) {
    next if ($line =~ m/^#/);
    next if (length($line) < 2);
    chomp $line;
    my ($accO, $asmIdO, $sciNameO, $taxIdO) = split('\t', $line);
    $sciNameOverride{$accO} = $sciNameO;
    $sciNameOverride{$asmIdO} = $sciNameO;
    $taxIdOverride{$accO} = $taxIdO;
    $taxIdOverride{$asmIdO} = $taxIdO;
  }
  close ($sn);
}

my $Name = shift;
my $asmHubName = shift;
my $defaultAssembly = shift;
my $inputList = shift;
my $orderList = $inputList;
if ( ! -s "$orderList" ) {
  $orderList = $toolsDir/$inputList;
}
my %cladeId;	# value is asmId, value is clade, useful for 'legacy' index page

printf STDERR "# mkHubIndex %s %s %s %s\n", $Name, $asmHubName, $defaultAssembly, $orderList;
my $hprcIndex = 0;
my $ccgpIndex = 0;
my $vgpIndex = 0;
my $brcIndex = 0;
$hprcIndex = 1 if ($Name =~ m/hprc/i);
$ccgpIndex = 1 if ($Name =~ m/ccgp/i);
$vgpIndex = 1 if ($Name =~ m/vgp/i);
$brcIndex = 1 if ($Name =~ m/brc/i);
my %extraClass;	# key is asmId, value is taxon 'class' as set by VGP project
if ($vgpIndex || $ccgpIndex) {
  my $whichIndex = "vgp";
  $whichIndex = "ccgp" if ($ccgpIndex);
  my $extraClass = "$home/kent/src/hg/makeDb/doc/${whichIndex}AsmHub/${whichIndex}.taxId.asmId.class.txt";
  open (FH, "<$extraClass") or die "can not read $extraClass";
  while (my $line = <FH>) {
    my ($taxId, $asmId, $class) = split('\t', $line);
    $extraClass{$asmId} = $class;
  }
  close (FH);
}

my @orderList;	# asmId of the assemblies in order from the *.list files
# the order to read the different .list files:
my $assemblyTotal = 0;
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
  print <<"END";
<!DOCTYPE HTML>
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
  if ($ccgpIndex) {
    print <<"END";
<!DOCTYPE HTML>
<!--#set var="TITLE" value="CCGP -  California Conservation Genomics Project " -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>CCGP -  California Conservation Genomics Project assembly hub</h1>
<p>
<a href='https://www.ccgproject.org/' target=_blank>
<img src='CCGP_logo.png' width=280 alt='CCGP logo'></a></p>
<p>
This assembly hub contains assemblies released
by the <a href='https://www.ccgproject.org/' target=_blank>
California Conservation Genomics Project.</a>
</p>

END
  } elsif ($hprcIndex) {
    print <<"END";
<!DOCTYPE HTML>
<!--#set var="TITLE" value="HPRC - Human Pangenome Reference Consortium" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>HPRC - Human Pangenome Reference Consortium assembly hub</h1>
<p>
<a href='https://humanpangenome.org/' target=_blank>
<img src='HPRC_logo.png' width=280 alt='HPRC logo'></a></p>
<p>
This assembly hub contains assemblies released
by the <a href='https://humanpangenome.org/' target=_blank>
Human Pangenome Reference Consortium.</a>
</p>

END
  } elsif ($brcIndex) {
    print <<"END";
<!DOCTYPE HTML>
<!--#set var="TITLE" value="BRC - Bioinformatics Research Center" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>BRC - Bioinformatics Research Center</h1>
<p>
<a href='https://brc-analytics.org/' target=_blank>
<img src='BRClogo.svg' height=26 alt='BRC logo'></a></p>
<p>
This site will provide data access to genomes and annotations for all
eukaryotic pathogens, host taxa, and vectors previously served by
VEuPathDB. This is a part of the BRC Analytics project funded by the NIAID.
For more information, see also:
<a href=' https://brc-analytics.org' target=_blank>brc-analytics.org</a>.
Access this assembly data in
<a href='assemblyList.json' target=_blank rel='noopener noreferrer' aria-label='Download the assembly data in JSON format'>
JSON format</a>.
</p>

END
  } else {
    print <<"END";
<!DOCTYPE HTML>
<!--#set var="TITLE" value="$Name genomes assembly hubs" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>$Name Genomes assembly hubs</h1>
<p>
Assemblies from NCBI/Genbank/Refseq sources, $subSetMessage.
</p>

END
  }
}

print <<"END";
<h3>How to view the assembly of interest</h3>
<p>
The links to the genome browser in the table below will attach that
one specific assembly to the genome browser.  Use the links in
the column labeled <b>common name and view in browser</b> to view that
assembly in the genome browser.
</p>

<h3>See also: <a href='asmStats.html'>assembly statistics</a>,&nbsp;<a href='trackData.html'>track statistics</a> &lt;== additional information for these assemblies.</h3><br>

<h3>Cite reference: To reference these resources in publications, please credit:</h3>
<p>
Clawson, H., Lee, B.T., Raney, B.J. et al.
"<b>GenArk: towards a million UCSC genome browsers</b>.<br><em>Genome Biol</em> 24, 217 (2023).
<a href='https://doi.org/10.1186/s13059-023-03057-x' target=_blank>
https://doi.org/10.1186/s13059-023-03057-x</a>
</p>
END

if ($vgpIndex) {
  if ( 1 == 0 ) {
  print <<"END";
<h3>Listings:</h3>&nbsp;&nbsp;<b>(from RepeatModeler masking)</b>
<p>
<ul>
<li><a href='modeler.families.urls.txt' target=_blank>families fasta.gz</a> list of URLs for the custom library created by the RepeatModeler run</li>
<li><a href='modeler.2bit.urls.txt' target=_blank>assembly 2bit file list</a> of URLs as masked with the RepeatModeler + <b>TRF/simpleRepeats</b> with period of 12 or less</li>
<li><a href='rmod.log.file.list.txt' target=_blank>the rmod.log files from each RepeatModeler run</a></li>
<li><a href='default.twoBit.file.list.txt' target=_blank>default GenArk 2bit file list</a> of URLs as masked with the ordinary RepeatMasker + <b>TRF/simpleRepeats</b> with period of 12 or less</li>
<li><a href='modeler.table.txt' target=_blank>this data table in tab-separated</a> file text format (including TBD not working yet, or in VGP collection but not on the alignment list)</li>
</ul>
</p>
END
  }
}

print <<"END";
<h3>Data resource links</h3>
NOTE: <em>Click on the column headers to sort the table by that column</em><br>
<br>
The <em>common name and view in browser</em> will attach only that single assembly to
the genome browser.<br>
The <em>scientific name and data download</em> link provides access to the files for that one
assembly hub.<br>
END

  if ($vgpIndex) {
    print <<"END";
The <em>class VGP link</em> provides access to the VGP GenomeArk page for that genome.<br>
END

  }

print <<"END";
The other links provide access to NCBI resources for these assemblies.
END

}	#	sub startHtml()

##############################################################################
### start the table output
##############################################################################
sub startTable() {
print '
<table class="sortable" border="1">
<thead style="position:sticky; top:0;"><tr><th>count</th>
  <th><span style="float: left;">common&nbsp;name&nbsp;and<br>view&nbsp;in&nbsp;UCSC&nbsp;browser</span><span style="float: right;">[IGV&nbsp;browser]</span></th>
  <th>scientific name<br>and&nbsp;data&nbsp;download</th>
  <th>NCBI&nbsp;assembly</th>
  <th>BioSample</th>
';
if ("viral" ne $asmHubName) {
  printf "  <th>BioProject</th>\n";
}

printf "<th>assembly&nbsp;date,<br>source&nbsp;link</th>\n";

if ("legacy" eq $asmHubName) {
  printf "<th>clade</th>\n";
}

if ($ccgpIndex) {
  printf "<th>class<br>CCGP&nbsp;link</th>\n";
} elsif ($vgpIndex) {
  printf "<th>class<br>VGP&nbsp;link</th>\n";
}
print "</tr></thead><tbody>\n";
}	#	sub startTable()

##############################################################################
### end the table output
##############################################################################
sub endTable() {

print <<"END";

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
    my $gcPrefix = "GCx";
    my $asmAcc = "asmAcc";
    my $asmName = "asmName";
    my $accessionId = "GCx_098765432.1";
    my $accessionDir = substr($asmId, 0 ,3);
    my $configRa = "n/a";
    if ($asmId !~ m/^GC/) {	# looking at a UCSC database, not a hub
       $configRa = "/hive/data/genomes/$asmId/$asmId.config.ra";
      $accessionId = `grep ^genBankAccessionID "${configRa}" | cut -d' ' -f2`;
       chomp $accessionId;
       $asmName = `grep ^ncbiAssemblyName "${configRa}" | cut -d' ' -f2`;
       chomp $asmName;
       $accessionDir = substr($accessionId, 0 ,3);
       $accessionDir .= "/" . substr($accessionId, 4 ,3);
       $accessionDir .= "/" . substr($accessionId, 7 ,3);
       $accessionDir .= "/" . substr($accessionId, 10 ,3);
       ($gcPrefix, $asmAcc) = split('_', $accessionId, 2);
       printf STDERR "# not an assembly hub: '%s' - '%s' '%s' '%s'\n", $asmId, $accessionId, $gcPrefix, $asmAcc;
    } else {
       ($gcPrefix, $asmAcc, $asmName) = split('_', $asmId, 3);
       $accessionDir .= "/" . substr($asmId, 4 ,3);
       $accessionDir .= "/" . substr($asmId, 7 ,3);
       $accessionDir .= "/" . substr($asmId, 10 ,3);
    }
    $accessionId = sprintf("%s_%s", $gcPrefix, $asmAcc);
    my $ncbiFtpLink = "https://ftp.ncbi.nlm.nih.gov/genomes/all/$accessionDir/$asmId";
   my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir/$asmId";
    my $asmReport="$buildDir/download/${asmId}_assembly_report.txt";
    if ($asmId =~ m/^GCA/) {
     $buildDir = "/hive/data/genomes/asmHubs/genbankBuild/$accessionDir/$asmId";
     $asmReport="$buildDir/download/${asmId}_assembly_report.txt";
    } elsif ($asmId !~ m/^GC/) {
       $buildDir="/hive/data/outside/ncbi/genomes/$accessionDir/${accessionId}_${asmName}";
       $asmReport="$buildDir/${accessionId}_${asmName}_assembly_report.txt";
    }
    my $trackDb="$buildDir/${asmId}.trackDb.txt";
#    next if (! -s "$trackDb");	# assembly build not complete
    my $commonName = "notFound(${asmId})";
    my $sciName = "notFound";
    my $bioSample = "notFound";
    my $bioProject = "notFound";
    my $taxId = "notFound";
    $taxId = $taxIdOverride{$accessionId} if (defined($taxIdOverride{$accessionId}));
    my $asmDate = "notFound";
    my $itemsFound = 0;
    if ( -s "${asmReport}" ) {
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
               $sciName = $sciNameOverride{$accessionId} if (defined($sciNameOverride{$accessionId}));
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
    } elsif ( -s "${configRa}" ) {	#	if ( -s "${asmReport}" )
# ncbiAssemblyName Sscrofa10.2
# genBankAccessionID GCA_000003025.4
# ncbiBioProject 13421
# assemblyDate Aug. 2011

       $asmName = `grep ^ncbiAssemblyName "${configRa}" | cut -d' ' -f2`;
       chomp $asmName;
       $commonName = `grep ^commonName "${configRa}" | cut -d' ' -f2-`;
       chomp $commonName;
       if (defined($taxIdOverride{$accessionId})) {
         $taxId = $taxIdOverride{$accessionId}
       } else {
         $taxId = `grep ^taxId "${configRa}" | cut -d' ' -f2`;
         chomp $taxId;
       }
       if (defined($sciNameOverride{$accessionId})) {
         $sciName = $sciNameOverride{$accessionId}
       } else {
         $sciName = `grep ^scientificName "${configRa}" | cut -d' ' -f2-`;
         chomp $sciName;
       }
       $asmDate = `grep ^assemblyDate "${configRa}" | cut -d' ' -f2-`;
       chomp $asmDate;
       $bioProject = `grep ^ncbiBioProject "${configRa}" | cut -d' ' -f2-`;
       chomp $bioProject;
       $bioSample = `grep ^ncbiBioSample "${configRa}" | cut -d' ' -f2-`;
       chomp $bioSample;
       $ncbiFtpLink = "https://ftp.ncbi.nlm.nih.gov/genomes/all/$accessionDir/${accessionId}_${asmName}";
    }
    my $hubUrl = "https://hgdownload.soe.ucsc.edu/hubs/$accessionDir/$accessionId";
    my $gbdbUrl = "/gbdb/genark/$accessionDir/$accessionId";
    my $browserName = $commonName;
    my $browserUrl = "https://genome.ucsc.edu/cgi-bin/hgTracks?genome=$accessionId&hubUrl=$gbdbUrl/hub.txt";
    if ($asmId !~ m/^GC/) {
       $hubUrl = "https://hgdownload.soe.ucsc.edu/goldenPath/$asmId/bigZips";
       $browserUrl = "https://genome.ucsc.edu/cgi-bin/hgTracks?db=$asmId";
       $browserName = "$commonName ($asmId)";
    }
    printf "<tr><td style='text-align: right;'>%d</td>\n", ++$rowCount;
    #  common name and view in browser
    if ( $asmId =~ m/^GC/ ) {
       my $hubTxt = "${hubUrl}/hub.txt";
       my $igvUrl = "https://igv.org/app/?hubURL=$hubTxt";
       printf "<td><span style='float: left;'><a href='%s' target=_blank>%s</a></span><span style='float: right;'>[<a href='%s' target=_blank>IGV</a>]</span></td>\n", $browserUrl, $browserName, $igvUrl;
    } else {
       printf "<td style='text-align: center;'><a href='%s' target=_blank>%s</a></td>\n", $browserUrl, $browserName;
    }
    # scientific name and data download
    printf "    <td style='text-align: center;'><a href='%s/' target=_blank>%s</a></td>\n", $hubUrl, $sciName;
    if ($asmId !~ m/^GC/) {
      printf "    <td style='text-align: left;'><a href='https://www.ncbi.nlm.nih.gov/assembly/%s_%s/' target=_blank>%s_%s</a></td>\n", $gcPrefix, $asmAcc, $accessionId, $asmName;
    } else {
      printf "    <td style='text-align: left;'><a href='https://www.ncbi.nlm.nih.gov/assembly/%s/' target=_blank>%s</a></td>\n", $accessionId, $asmId;
    }
    # viruses do not appear to have BioSample
    if ($asmHubName ne "viral") {
      if ( $bioSample ne "notFound" ) {
        printf "    <td style='text-align: left;'><a href='https://www.ncbi.nlm.nih.gov/biosample/?term=%s' target=_blank>%s</a></td>\n", $bioSample, $bioSample;
      } else {
      printf "    <td style='text-align: left;'>n/a</td>\n";
      }
    }
    # one broken assembly_report
    $bioProject= "PRJEB25768" if ($accessionId eq "GCA_900324465.2");
    if ($bioProject eq "notFound") {
      printf "    <td style='text-align: left;'>%s</td>\n", $bioProject;
    } else {
      printf "    <td style='text-align: left;'><a href='https://www.ncbi.nlm.nih.gov/bioproject/?term=%s' target=_blank>%s</a></td>\n", $bioProject, $bioProject;
    }
    printf "    <td style='text-align: center;'><a href='%s' target=_blank>%s</a></td>\n", $ncbiFtpLink, $asmDate;
    if ("legacy" eq $asmHubName) {
      if (! defined($cladeId{$asmId})) {
         printf STDERR "# ERROR: missing clade definition for %s\n", $asmId;
         exit 255;
      } else {
         printf "    <td style='text-align: center;'>%s</td>\n", $cladeId{$asmId};
      }
    }
    if ($ccgpIndex) {
      my $sciNameUnderscore = $sciName;
      $sciNameUnderscore =~ s/ /_/g;
      $sciNameUnderscore = "Strigops_habroptilus" if ($sciName =~ m/Strigops habroptila/);

      if (! defined($extraClass{$asmId})) {
         printf STDERR "# ERROR: no 'class' defined for CCGP assembly %s\n", $asmId;
         exit 255;
      }
# it isn't clear how we can get these names
# https://www.ccgproject.org/species/corynorhinus-townsendii-townsends-big-eared-bat
      printf "    <td style='text-align: center;'><a href='https://www.ccgproject.org/species/%s/' target=_blank>%s</a></td>\n", $sciNameUnderscore, $extraClass{$asmId}
    } elsif ($vgpIndex) {
      my $sciNameUnderscore = $sciName;
      $sciNameUnderscore =~ s/ /_/g;
      $sciNameUnderscore = "Strigops_habroptilus" if ($sciName =~ m/Strigops habroptila/);

      if (! defined($extraClass{$asmId})) {
         printf STDERR "# ERROR: no 'class' defined for VGP/CCGP assembly %s\n", $asmId;
         exit 255;
      }
      printf "    <td style='text-align: center;'><a href='https://vgp.github.io/genomeark/%s/' target=_blank>%s</a></td>\n", $sciNameUnderscore, $extraClass{$asmId}
    }
    printf "</tr>\n";
  }
}	#	sub tableContents()

##############################################################################
### main()
##############################################################################

# if there is a 'promoted' list, it has been taken out of the 'orderList'
# so will need to stuff it back in at the correct ordered location
my %promotedList;	# key is asmId, value is common name
my $promotedList = dirname(${orderList}) . "/promoted.list";
my @promotedList;	# contents are asmIds, in order by lc(common name)
my $promotedIndex = -1;	# to walk through @promotedList;

if ( -s "${promotedList}" ) {
  open (FH, "<${promotedList}" ) or die "can not read ${promotedList}";
  while (my $line = <FH>) {
    next if ($line =~ m/^#/);
    chomp $line;
    my ($asmId, $commonName) = split('\t', $line);
    $promotedList{$asmId} = $commonName;
  }
  close (FH);
  foreach my $asmId ( sort { lc($promotedList{$a}) cmp lc($promotedList{$b}) } keys %promotedList) {
     push @promotedList, $asmId;
  }
  $promotedIndex = 0;
}

my $cladeList = dirname(${orderList}) . "/$asmHubName.clade.txt";
if ( -s "${cladeList}" ) {
  open (FH, "<$cladeList") or die "can not read ${cladeList}";
  while (my $clade = <FH>) {
    chomp $clade;
    my @a = split('\t', $clade);
    $cladeId{$a[0]} = $a[1];
  }
  close (FH);
}

open (FH, "<${orderList}") or die "can not read ${orderList}";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my ($asmId, $commonName) = split('\t', $line);
  if ( ($promotedIndex > -1) && ($promotedIndex < scalar(@promotedList))) {
     my $checkInsertAsmId = $promotedList[$promotedIndex];
     my $checkInsertName = $promotedList{$checkInsertAsmId};
     # insert before this commonName when alphabetic before
     if (lc($checkInsertName) lt lc($commonName)) {
       push @orderList, $checkInsertAsmId;
       $commonName{$checkInsertAsmId} = $checkInsertName;
       ++$assemblyTotal;
       printf STDERR "# inserting '%s' before '%s' at # %03d\n", $checkInsertName, $commonName, $assemblyTotal;
       ++$promotedIndex;	# only doing one at this time
                        # TBD: will need to improve this for more inserts
     }
  }
  push @orderList, $asmId;
  $commonName{$asmId} = $commonName;
  ++$assemblyTotal;
}
close (FH);
# TBD: and would need to check if all promoted assemblies have been included

startHtml();
startTable();
tableContents();
endTable();
endHtml();
