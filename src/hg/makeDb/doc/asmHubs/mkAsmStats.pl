#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;
use FindBin qw($Bin);
use lib "$Bin";
use commonHtml;
use File::stat;

my $argc = scalar(@ARGV);
if ($argc != 3) {
  printf STDERR "mkAsmStats Name asmHubName [two column name list]\n";
  printf STDERR "e.g.: mkAsmStats Mammals mammals mammals.asmId.commonName.tsv\n";
  printf STDERR "the name list is found in \$HOME/kent/src/hg/makeDb/doc/asmHubs/\n";
  printf STDERR "\nthe two columns are 1: asmId (accessionId_assemblyName)\n";
  printf STDERR "column 2: common name for species, columns separated by tab\n";
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
my $inputList = shift;
my $orderList = $inputList;
if ( ! -s "$orderList" ) {
  $orderList = $toolsDir/$inputList;
}

my @orderList;	# asmId of the assemblies in order from the orderList file
my %commonName;	# key is asmId, value is a common name, perhaps more appropriate
                # than found in assembly_report file
my $vgpIndex = 0;
$vgpIndex = 1 if ($Name =~ m/vgp/i);
my $hprcIndex = 0;
$hprcIndex = 1 if ($Name =~ m/hprc/i);
my $brcIndex = 0;
$brcIndex = 1 if ($Name =~ m/brc/i);

my $assemblyTotal = 0;	# complete list of assemblies in this group
my $asmCount = 0;	# count of assemblies completed and in the table
my $overallNucleotides = 0;
my $overallSeqCount = 0;
my $overallGapSize = 0;
my $overallGapCount = 0;

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
<!DOCTYPE HTML>
<!--#set var="TITLE" value="VGP - Vertebrate Genomes Project assembly hubs, assembly statistics" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>VGP - Vertebrate Genomes Project assembly hubs, assembly statistics</h1>
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
  if ($hprcIndex) {
    print <<"END"
<!DOCTYPE HTML>
<!--#set var="TITLE" value="HPRC - Human Pangenome Reference Consortium assembly hubs, assembly statistics" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>VGP - Human Pangenome Reference Consortium assembly hubs, assembly statistics</h1>
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
<!--#set var="TITLE" value="BRC - Bioinformatics Research Center - assembly statistics" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>BRC - Bioinformatics Research Center - assembly statistics</h1>
<p>
<a href='https://brc-analytics.org/' target=_blank>
<img src='BRClogo.svg' height=26 alt='BRC logo'></a></p>
<p>
This site will provide data access to genomes and annotations for all
eukaryotic pathogens, host taxa, and vectors previously served by
VEuPathDB. This is a part of the BRC Analytics project funded by the NIAID.
For more information, see also:
<a href=' https://brc-analytics.org' target=_blank>brc-analytics.org</a>
</p>

END
  } else {
    print <<"END"
<!DOCTYPE HTML>
<!--#set var="TITLE" value="$Name genomes assembly hubs, assembly statistics" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>$Name Genomes assembly hubs, assembly statistics</h1>
<p>
Assemblies from NCBI/Genbank/Refseq sources, $subSetMessage.
</p>

END
  }
}

  print <<"END"
<h3>See also: <a href='index.html'>hub access</a>,&nbsp;<a href='trackData.html'>track statistics</a></h3><br>

<h3>Data resource links</h3>
NOTE: <em>Click on the column headers to sort the table by that column</em><br>
The <em>link to genome browser</em> will attach only that single assembly to
the genome browser.
END
}

##############################################################################
### start the table output
##############################################################################
sub startTable() {
print <<"END"
<table class="sortable" border="1">
<thead style="position:sticky; top:0;"><tr><th>count</th>
  <th>common name<br>link&nbsp;to&nbsp;genome&nbsp;browser</th>
  <th>scientific name<br>and&nbsp;data&nbsp;download</th>
  <th>NCBI&nbsp;assembly</th>
  <th>sequence<br>count</th><th>genome&nbsp;size<br>nucleotides</th>
  <th>gap<br>count</th><th>unknown&nbsp;bases<br>(gap size sum)</th><th>masking<br>percent</th>
</tr></thead><tbody>
END
}

##############################################################################
### end the table output
##############################################################################
sub endTable() {

my $commaNuc = commify($overallNucleotides);
my $commaSeqCount = commify($overallSeqCount);
my $commaGapSize = commify($overallGapSize);
my $commaGapCount = commify($overallGapCount);

my $percentDone = 100.0 * $asmCount / $assemblyTotal;
my $doneMsg = "";
if ($asmCount < $assemblyTotal) {
  $doneMsg = sprintf(" (%d build completed, %.2f %% finished)", $asmCount, $percentDone);
}

if ($assemblyTotal > 1) {
  print "
</tbody>
<tfoot><tr><th>TOTALS:</th><td style='text-align: center;' colspan=3>total assembly count&nbsp;${assemblyTotal}${doneMsg}</td>
  <td style='text-align: right;'>$commaSeqCount</td>
  <td style='text-align: right;'>$commaNuc</td>
  <td style='text-align: right;'>$commaGapCount</td>
  <td style='text-align: right;'>$commaGapSize</td>
  <td colspan=1>&nbsp;</td>
  </tr>
";

  # try extra column headers as last row for this very large index page
  if ($vgpIndex && ($asmCount > 15)) {
  print "<tr><th>count</th>
  <th>common name<br>link&nbsp;to&nbsp;genome&nbsp;browser</th>
  <th>scientific name<br>and&nbsp;data&nbsp;download</th>
  <th>NCBI&nbsp;assembly</th>
  <th>sequence<br>count</th><th>genome&nbsp;size<br>nucleotides</th>
  <th>gap<br>count</th><th>unknown&nbsp;bases<br>(gap size sum)</th><th>masking<br>percent</th>
  </tr>
";
  }

  print "
</tfoot>
</table>
";

  } else {
  print "
</tbody>
</table>
";
  }	# $assemblyTotal <= 1
}	#	sub endTable()

##############################################################################
### end the HTML output
##############################################################################
sub endHtml() {

&commonHtml::otherHubLinks($vgpIndex, $asmHubName);
&commonHtml::htmlFooter($vgpIndex, $asmHubName);

}

sub asmCounts($) {
  my ($chromSizes) = @_;
  my ($sequenceCount, $totalSize) = split('\s+', `ave -col=2 $chromSizes | egrep "^count|^total" | awk '{printf "%d\\n", \$NF}' | xargs echo`);
  return ($sequenceCount, $totalSize);
}

sub maskStats($) {
  my ($faSizeFile) = @_;
  my $gapSize = `grep 'sequences in 1 file' $faSizeFile | awk '{print \$3}'`;
  chomp $gapSize;
  $gapSize =~ s/\(//;
  my $totalBases = `grep 'sequences in 1 file' $faSizeFile | awk '{print \$1}'`;
  chomp $totalBases;
  my $maskedBases = `grep 'sequences in 1 file' $faSizeFile | awk '{print \$9}'`;
  chomp $maskedBases;
  my $maskPerCent = 100.0 * $maskedBases / $totalBases;
  return ($gapSize, $maskPerCent);
}

# grep "sequences in 1 file" GCA_900324465.2_fAnaTes1.2.faSize.txt
# 555641398 bases (3606496 N's 552034902 real 433510637 upper 118524265 lower) in 50 sequences in 1 files

sub gapStats($$) {
  my ($buildDir, $asmId) = @_;
  my $gapBed = "$buildDir/trackData/allGaps/$asmId.allGaps.bed.gz";
  my $gapCount = 0;
  if ($asmId !~ m/^GC/) {
     $gapBed = "/hive/data/genomes/$asmId/$asmId.N.bed";
     if ( -s "$gapBed" ) {
       $gapCount = `awk '{print \$3-\$2}' $gapBed | ave stdin | grep '^count' | awk '{print \$2}'`;
     }
  } elsif ( -s "$gapBed" ) {
    $gapCount = `zcat $gapBed | awk '{print \$3-\$2}' | ave stdin | grep '^count' | awk '{print \$2}'`;
  }
  chomp $gapCount;
  return ($gapCount);
}

##############################################################################
### tableContents()
##############################################################################
sub tableContents() {

  foreach my $asmId (@orderList) {
    my $gcPrefix = "GCx";
    my $asmAcc = "asmAcc";
    my $asmName = "asmName";
    my $accessionId = "GCx_098765432.1";
    my $accessionDir = "";
    my $configRa = "n/a";
    if ($asmId !~ m/^GC/) {
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
       printf STDERR "# %03d\t%s_%s (%s)\n", 1 + $asmCount, $accessionId, $asmName, $asmId;
    } else {
       ($gcPrefix, $asmAcc, $asmName) = split('_', $asmId, 3);
       $accessionId = sprintf("%s_%s", $gcPrefix, $asmAcc);
       $accessionDir = substr($asmId, 0 ,3);
       $accessionDir .= "/" . substr($asmId, 4 ,3);
       $accessionDir .= "/" . substr($asmId, 7 ,3);
       $accessionDir .= "/" . substr($asmId, 10 ,3);
       printf STDERR "# %03d\t%s\n", 1 + $asmCount, $asmId;
    }
    # assume GCF/refseq build
   my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir/$asmId";
    if ($gcPrefix eq "GCA") {   # this is a GCA/genbank build
     $buildDir = "/hive/data/genomes/asmHubs/genbankBuild/$accessionDir/$asmId";
    }
    my $asmReport="$buildDir/download/${asmId}_assembly_report.txt";
    my $chromSizes = "${buildDir}/${asmId}.chrom.sizes";
    my $twoBit = "${buildDir}/trackData/addMask/${asmId}.masked.2bit";
    my $faSizeTxt = "${buildDir}/${asmId}.faSize.txt";
    if ($asmId !~ m/^GC/) {        # this is a UCSC genome database browser
       $buildDir="/hive/data/outside/ncbi/genomes/$accessionDir/${accessionId}_${asmName}";
       $asmReport="$buildDir/${accessionId}_${asmName}_assembly_report.txt";
       $chromSizes = "/hive/data/genomes/$asmId/chrom.sizes";
       $twoBit = "/hive/data/genomes/$asmId/$asmId.2bit";
       $faSizeTxt = "/hive/data/genomes/$asmId/faSize.${asmId}.2bit.txt";
    }
    if (! -s "$asmReport") {
      printf STDERR "# no assembly report:\n# %s\n", $asmReport;
      next;
    }
    if (! -s "$twoBit") {
      printf STDERR "# no 2bit file:\n# %s\n", $twoBit;
      next;
    }
    if ( ! -s "$faSizeTxt" ) {
       printf STDERR "twoBitToFa $twoBit stdout | faSize stdin > $faSizeTxt\n";
       print `twoBitToFa $twoBit stdout | faSize stdin > $faSizeTxt`;
    }
    my ($gapSize, $maskPerCent) = maskStats($faSizeTxt);
    $overallGapSize += $gapSize;
    my ($seqCount, $totalSize) = asmCounts($chromSizes);
    $overallSeqCount += $seqCount;
    $overallNucleotides += $totalSize;
    my $gapCount = gapStats($buildDir, $asmId);
    $overallGapCount += $gapCount;
    my $sciName = "notFound";
    my $commonName = "notFound";
    my $bioSample = "notFound";
    my $bioProject = "notFound";
    my $taxId = "notFound";
    $taxId = $taxIdOverride{$accessionId} if (defined($taxIdOverride{$accessionId}));
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
           $asmDate = $line;
           $asmDate =~ s/.*:\s+//;
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
    my $hubUrl = "https://hgdownload.soe.ucsc.edu/hubs/$accessionDir/$accessionId";
    my $browserName = $commonName;
    my $browserUrl = "https://genome.ucsc.edu/h/$accessionId";
    if ($asmId !~ m/^GC/) {
       $hubUrl = "https://hgdownload.soe.ucsc.edu/goldenPath/$asmId/bigZips";
       $browserUrl = "https://genome.ucsc.edu/cgi-bin/hgTracks?db=$asmId";
       $browserName = "$commonName ($asmId)";
    }
    printf "<tr><td style='text-align: right;'>%d</td>\n", ++$asmCount;
#    printf "<td align=center><a href='https://genome.ucsc.edu/cgi-bin/hgGateway?hubUrl=%s/hub.txt&amp;genome=%s&amp;position=lastDbPos' target=_blank>%s</a></td>\n", $hubUrl, $accessionId, $commonName;
    printf "<td style='text-align: center;'><a href='%s' target=_blank>%s</a></td>\n", $browserUrl, $browserName;
    printf "    <td style='text-align: center;'><a href='%s/' target=_blank>%s</a></td>\n", $hubUrl, $sciName;
    if ($asmId !~ m/^GC/) {
      printf "    <td style='text-align: left;'><a href='https://www.ncbi.nlm.nih.gov/assembly/%s_%s/' target=_blank>%s_%s</a></td>\n", $gcPrefix, $asmAcc, $accessionId, $asmName;
    } else {
      printf "    <td style='text-align: left;'><a href='https://www.ncbi.nlm.nih.gov/assembly/%s/' target=_blank>%s</a></td>\n", $accessionId, $asmId;
    }
    printf "    <td style='text-align: right;'>%s</td>\n", commify($seqCount);
    printf "    <td style='text-align: right;'>%s</td>\n", commify($totalSize);
    printf "    <td style='text-align: right;'>%s</td>\n", commify($gapCount);
    printf "    <td style='text-align: right;'>%s</td>\n", commify($gapSize);
    printf "    <td style='text-align: right;'>%.2f</td>\n", $maskPerCent;
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

startHtml();
startTable();
tableContents();
endTable();
endHtml();
