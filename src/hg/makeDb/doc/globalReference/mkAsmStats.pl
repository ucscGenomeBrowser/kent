#!/usr/bin/env perl

use strict;
use warnings;
use File::stat;

my $asmHubWorkDir = "globalReference";

my @orderList;	# asmId of the assemblies in order from the *.list files
# the order to read the different .list files:
my @classList = qw( human );
my %class;	# key is asmId, value is from class list
my $assemblyCount = 0;
my $overallNucleotides = 0;
my $overallSeqCount = 0;
my $overallGapSize = 0;
my $overallGapCount = 0;

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

print <<"END"
<!DOCTYPE HTML 4.01 Transitional>
<!--#set var="TITLE" value="Global Reference Genomes Project assembly statistics" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>Global Reference Genomes Project assembly statistics</h1>
<p>
This assembly hub contains assemblies collected by the 
<a href="https://www.genome.wustl.edu/items/reference-genome-improvement/" target=_blank>Reference
Genome Improvement Project</a> from Washington University in Saint Louis, Missouri, USA. For more
information, contact 
<a href="https://www.genome.wustl.edu/people/tina-graves-lindsay/" target=_blank>
Tina Graves-Lindsay</a>.</p>

<p>
<h3>See also: <a href='index.html' target=_blank>hub access</a></h3>
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
<thead><tr><th>count</th><th>ethnicity<br>link&nbsp;to&nbsp;genome&nbsp;browser</th>
  <th>country&nbsp;of&nbsp;origin<br>and&nbsp;data&nbsp;download</th>
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

print <<"END"

</tbody>
<tfoot><tr><th>TOTALS:</th><td align=center colspan=3>assembly count&nbsp;$assemblyCount</td>
  <td align=right>$commaSeqCount</td>
  <td align=right>$commaNuc</td>
  <td align=right>$commaGapCount</td>
  <td align=right>$commaGapSize</td>
  <td colspan=1>&nbsp;</td>
  </tr></tfoot>
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

sub gapStats($) {
  my ($asmId) = @_;
  my $gapBed = "$asmId/trackData/allGaps/$asmId.allGaps.bed.gz";
  my $gapCount = 0;
  if ( -s "$gapBed" ) {
    $gapCount = `zcat $gapBed | awk '{print \$3-\$2}' | ave stdin | grep '^count' | awk '{print \$2}'`;
  }
  chomp $gapCount;
  return ($gapCount);
}

# GCA_901709675.1_fSynAcu1.1/trackData/allGaps] zcat GCA_901709675.1_fSynAcu1.1.allGaps.bed.gz | awk '{print $3-$2,$0}' | ave stdin | grep "^count" | awk '{print $2}'

##############################################################################
### tableContents()
##############################################################################
sub tableContents() {

  my $asmCount = 0;
  foreach my $asmId (reverse(@orderList)) {
#    next if ($asmId =~ m/GCF_900963305.1_fEcheNa1.1/);
    my $asmReport="${asmId}/download/${asmId}_assembly_report.txt";
    my ($gcPrefix, $asmAcc, $asmName) = split('_', $asmId, 3);
    my $chromSizes = "${asmId}/${asmId}.chrom.sizes";
    my $twoBit = "${asmId}/trackData/addMask/${asmId}.masked.2bit";
    my $faSizeTxt = "${asmId}/${asmId}.faSize.txt";
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
    my $gapCount = gapStats($asmId);
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
    printf "<tr><th>%d</th><td align=center><a href='https://genome.ucsc.edu/cgi-bin/hgGateway?hubUrl=https://hgdownload.soe.ucsc.edu/hubs/%s/hub.txt&amp;genome=%s&amp;position=lastDbPos' target=_blank>%s</a></td>\n", ++$asmCount, $asmHubWorkDir, $asmId, $ethnicGroup{$asmId};
    printf "    <td align=center><a href='https://hgdownload.soe.ucsc.edu/hubs/%s/genomes/%s/' target=_blank>%s</a></td>\n", $asmHubWorkDir, $asmId, $countryOfOrigin{$asmId};
    printf "    <td align=left><a href='https://www.ncbi.nlm.nih.gov/assembly/%s_%s/' target=_blank>%s</a></td>\n", $gcPrefix, $asmAcc, $asmId;
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

my $home = $ENV{'HOME'};
my $srcDir = "$home/kent/src/hg/makeDb/doc/$asmHubWorkDir";

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
