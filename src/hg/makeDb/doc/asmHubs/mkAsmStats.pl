#!/usr/bin/env perl

use strict;
use warnings;
use File::stat;

my $argc = scalar(@ARGV);
if ($argc != 2) {
  printf STDERR "mkAsmStats Name asmName\n";
  printf STDERR "e.g.: mkAsmStats Mammals mammals\n";
  exit 255;
}
my $Name = shift;
my $asmHubName = shift;

my $home = $ENV{'HOME'};
my $toolsDir = "$home/kent/src/hg/makeDb/doc/asmHubs";

my $commonNameList = "$asmHubName.asmId.commonName.tsv";
my $commonNameOrder = "$asmHubName.commonName.asmId.orderList.tsv";
my @orderList;	# asmId of the assemblies in order from the *.list files
# the order to read the different .list files:
my %betterName;	# key is asmId, value is better common name than found in
		# assembly_report

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

if ($Name =~ m/vgp/i) {
  print <<"END"
<!DOCTYPE HTML 4.01 Transitional>
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
Vertebrate Genomes Project.</a>
</p>

END
} else {
  print <<"END"
<!DOCTYPE HTML 4.01 Transitional>
<!--#set var="TITLE" value="$Name genomes assembly hubs, assembly statistics" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>$Name Genomes assembly hubs, assembly statistics</h1>
<p>
Assemblies from NCBI/Genbank/Refseq sources, $subSetMessage.
</p>

END
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
<thead><tr><th>count</th>
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
  print <<"END"

</tbody>
<tfoot><tr><th>TOTALS:</th><td align=center colspan=3>total assembly count&nbsp;${assemblyTotal}${doneMsg}</td>
  <td align=right>$commaSeqCount</td>
  <td align=right>$commaNuc</td>
  <td align=right>$commaGapCount</td>
  <td align=right>$commaGapSize</td>
  <td colspan=1>&nbsp;</td>
  </tr></tfoot>
</table>
END
} else {
  print <<"END"

</tbody>
</table>
END
}
}	#	sub endTable()

##############################################################################
### end the HTML output
##############################################################################
sub endHtml() {

if ($asmHubName ne "viral") {
  printf "<p>\n<table border='1'><thead>\n";
  printf "<tr><th colspan=6 style='text-align:center;'>Additional hubs with collections of assemblies</th></tr>\n";
  printf "<tr><th>Assembly hubs index pages:&nbsp;</th>\n";
  printf "<th><a href='../primates/index.html'>Primates</a></th>\n";
  printf "<th><a href='../mammals/index.html'>Mammals</a></th>\n";
  printf "<th><a href='../birds/index.html'>Birds</a></th>\n";
  printf "<th><a href='../fish/index.html'>Fish</a></th>\n";
  printf "<th><a href='../vertebrate/index.html'>other vertebrates</a></th>\n";

  printf "</tr><tr>\n";
  printf "<th>Hubs assembly statistics:&nbsp;</th>\n";
  printf "<th><a href='../primates/asmStats.html'>Primates</a></th>\n";
  printf "<th><a href='../mammals/asmStats.html'>Mammals</a></th>\n";
  printf "<th><a href='../birds/asmStats.html'>Birds</a></th>\n";
  printf "<th><a href='../fish/asmStats.html'>Fish</a></th>\n";
  printf "<th><a href='../vertebrate/asmStats.html'>other vertebrates</a></th>\n";

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
}

sub asmCounts($) {
  my ($chromSizes) = @_;
  my ($sequenceCount, $totalSize) = split('\s+', `ave -col=2 $chromSizes | egrep "^count|^total" | awk '{printf "%d\\n", \$NF}' | xargs echo`);
  return ($sequenceCount, $totalSize);
}

#    my ($gapSize) = maskStats($faSizeTxt);
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
  if ( -s "$gapBed" ) {
    $gapCount = `zcat $gapBed | awk '{print \$3-\$2}' | ave stdin | grep '^count' | awk '{print \$2}'`;
  }
  chomp $gapCount;
  return ($gapCount);
}

##############################################################################
### tableContents()
##############################################################################
sub tableContents() {

  foreach my $asmId (reverse(@orderList)) {
    my ($gcPrefix, $asmAcc, $asmName) = split('_', $asmId, 3);
    my $accessionId = sprintf("%s_%s", $gcPrefix, $asmAcc);
    my $accessionDir = substr($asmId, 0 ,3);
    $accessionDir .= "/" . substr($asmId, 4 ,3);
    $accessionDir .= "/" . substr($asmId, 7 ,3);
    $accessionDir .= "/" . substr($asmId, 10 ,3);
    my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir/$asmId";
    if ($gcPrefix eq "GCA") {
     $buildDir = "/hive/data/genomes/asmHubs/genbankBuild/$accessionDir/$asmId";
    }
    my $asmReport="$buildDir/download/${asmId}_assembly_report.txt";
    if (! -s "$asmReport") {
      printf STDERR "# no assembly report:\n# %s\n", $asmReport;
      next;
    }
    my $chromSizes = "${buildDir}/${asmId}.chrom.sizes";
    my $twoBit = "${buildDir}/trackData/addMask/${asmId}.masked.2bit";
    if (! -s "$twoBit") {
      printf STDERR "# no 2bit file:\n# %s\n", $twoBit;
      next;
    }
    my $trackDb="$buildDir/${asmId}.trackDb.txt";
    next if (! -s "$trackDb");	# assembly build not complete
    my $faSizeTxt = "${buildDir}/${asmId}.faSize.txt";
    if ( ! -s "$faSizeTxt" ) {
       printf STDERR "twoBitToFa $twoBit stdout | faSize stdin > $faSizeTxt\n";
       print `twoBitToFa $twoBit stdout | faSize stdin > $faSizeTxt`;
    }
    my ($gapSize, $maskPerCent) = maskStats($faSizeTxt);
    $overallGapSize += $gapSize;
    my ($seqCount, $totalSize) = asmCounts($chromSizes);
    $overallSeqCount += $seqCount;
#    my $totalSize=`ave -col=2 $chromSizes | grep "^total" | awk '{printf "%d", \$NF}'`;
    $overallNucleotides += $totalSize;
    my $gapCount = gapStats($buildDir, $asmId);
    $overallGapCount += $gapCount;
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
    printf "<tr><td align=right>%d</td>\n", ++$asmCount;
#    printf "<td align=center><a href='https://genome.ucsc.edu/cgi-bin/hgGateway?hubUrl=%s/hub.txt&amp;genome=%s&amp;position=lastDbPos' target=_blank>%s</a></td>\n", $hubUrl, $accessionId, $commonName;
    printf "<td align=center><a href='https://genome.ucsc.edu/h/%s' target=_blank>%s</a></td>\n", $accessionId, $commonName;
    printf "    <td align=center><a href='%s/' target=_blank>%s</a></td>\n", $hubUrl, $sciName;
    printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/assembly/%s/' target=_blank>%s</a></td>\n", $accessionId, $asmId;
    printf "    <td align=right>%s</td>\n", commify($seqCount);
    printf "    <td align=right>%s</td>\n", commify($totalSize);
    printf "    <td align=right>%s</td>\n", commify($gapCount);
    printf "    <td align=right>%s</td>\n", commify($gapSize);
    printf "    <td align=right>%.2f</td>\n", $maskPerCent;
    printf "</tr>\n";
  }
}

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
  ++$assemblyTotal;
}
close (FH);

startHtml();
startTable();
tableContents();
endTable();
endHtml();
