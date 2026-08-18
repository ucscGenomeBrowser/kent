#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;
use FindBin qw($Bin);
use lib "$Bin";
use commonHtml;
use File::stat;

my $argc = scalar(@ARGV);
if ($argc != 5) {
  printf STDERR "usage: trackData.pl Name asmHubName orderList prodOutFile testOutFile\n";
  printf STDERR "e.g.: trackData.pl Mammals mammals mammals.asmId.commonName.tsv trackData.html testTrackData.html\n";
  printf STDERR "the name list is found in \$HOME/kent/src/hg/makeDb/doc/asmHubs/\n";
  printf STDERR "\nthe two columns in the name list are 1: asmId (accessionId_assemblyName)\n";
  printf STDERR "column 2: common name for species, columns separated by tab\n";
  printf STDERR "\nWrites both the production and the '-test' variant of the track\n";
  printf STDERR "statistics page in a single pass over the assembly list -- each\n";
  printf STDERR "assembly's per-track stats (bigWigInfo/bigBedInfo/faSize etc.) are\n";
  printf STDERR "measured once and reused for both pages, instead of running the\n";
  printf STDERR "whole scan twice.\n";
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
my $prodOutFile = shift;
my $testOutFile = shift;
my $orderList = $inputList;
if ( ! -s "$orderList" ) {
  $orderList = $toolsDir/$inputList;
}

my @orderList;	# asmId of the assemblies in order from the orderList file
my %commonName;	# key is asmId, value is a common name, perhaps more appropriate
                # than found in assembly_report file
		# assembly_report
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
# capture(&): run a block of code that 'print's/'printf's, and return
# everything it printed as a string, instead of letting it go to STDOUT.
# This lets startHtml()/startTable()/endTable()/endHtml() keep their
# original print-based bodies unchanged, while the caller decides which
# of the two output files (or both) a given fragment belongs in.
##############################################################################
sub capture(&) {
  my ($code) = @_;
  my $buf = '';
  open(my $fh, '>', \$buf) or die "capture: $!";
  my $old = select($fh);
  $code->();
  select($old);
  close($fh);
  return $buf;
}

# ($itemCount, $percentCover) = bigWigMeasure($trackFile, $genomeSize);
sub bigWigMeasure($$) {
  my ($file, $genomeSize) = @_;
  my $bigWigInfo = `bigWigInfo "$file" | egrep "basesCovered:|mean:" | awk '{print \$NF}' | xargs echo | sed -e 's/,//g;'`;
  chomp $bigWigInfo;
  my ($bases, $mean) = split('\s+', $bigWigInfo);
  my $itemCount = sprintf ("%.2f", $mean);
  my $percentCover = sprintf("%.2f %%", 100.0 * $bases / $genomeSize);
  return ($itemCount, $percentCover);
}

# $percentCover = pcFbFile($trackFb);
sub pcFbFile($) {
  my ($trackFb) = @_;
  my ($itemBases, undef, undef, $noGapSize, undef) = split('\s+', `cat $trackFb`, 5);
  my $percentCover = sprintf("%.2f %%", 100.0 * $itemBases / $noGapSize);
  return $percentCover;
}

# ($itemCount, $percentCover) = oneTrackData($asmId, $track, $trackFile, $totalSize, $trackFb, $runDir);
# might have a track feature bits file (trackFb), maybe not
sub oneTrackData($$$$$$) {
  my ($asmId, $trackName, $file, $genomeSize, $trackFb, $runDir) = @_;
# printf STDERR "# %s\n", $file;
  my $itemCount = 0;
  my $percentCover = 0;
  if (! -s "${file}") {
    if ($trackName eq "gapOverlap") {
      if (-s "${runDir}/$asmId.gapOverlap.bed.gz" ) {
       my $lineCount=`zcat "${runDir}/$asmId.gapOverlap.bed.gz" | head | wc -l`;
        chomp $lineCount;
       if (0 == $lineCount) {
         return("0", "0 %");
       } else {
         return("n/a", "n/a");
       }
      }
    } elsif ($trackName eq "gap") {
      return("0", "0 %");
    } else {
      return("n/a", "n/a");
    }
  } else {
    if ($file =~ m/.bw$/) {
      ($itemCount, $percentCover) = bigWigMeasure($file, $genomeSize);
    } else {
      my $bigBedInfo = `bigBedInfo "$file" | egrep "itemCount:|basesCovered:" | awk '{print \$NF}' | xargs echo | sed -e 's/,//g;'`;
      chomp $bigBedInfo;
      my ($items, $bases) = split('\s', $bigBedInfo);
      $itemCount = commify($items);
      $percentCover = sprintf("%.2f %%", 100.0 * $bases / $genomeSize);
      if ( -s "${trackFb}" ) {
	$percentCover = pcFbFile($trackFb);
      }
# printf STDERR "# bigBedInfo %s %s %s\n", $itemCount, $percentCover, $file;
    }
  }
  return ($itemCount, $percentCover);
}	#	sub oneTrackData($$$$$$)

##############################################################################
### start the HTML output -- identical for the prod and -test pages
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
<!--#set var="TITLE" value="VGP - Vertebrate Genomes Project assembly hubs, track statistics" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>VGP - Vertebrate Genomes Project assembly hubs, track statistics</h1>
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
<!--#set var="TITLE" value="HPRC - Human Pangenome Reference Consortium assembly hubs, track statistics" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>HPRC - Human Pangenome Reference Consortium assembly hubs, track statistics</h1>
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
<!--#set var="TITLE" value="BRC - Bioinformatics Research Center - track statistics" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>BRC - Bioinformatics Research Center - track statistics</h1>
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
<!--#set var="TITLE" value="$Name genomes assembly hubs, track statistics" -->
<!--#set var="ROOT" value="../.." -->

<!--#include virtual="\$ROOT/inc/gbPageStartHardcoded.html" -->

<h1>$Name Genomes assembly hubs, track statistics</h1>
<p>
Assemblies from NCBI/Genbank/Refseq sources, $subSetMessage.
</p>

END
  }
}
  my $indexUrl = "index";
  my $asmStats = "asmStats";
  print <<"END"
<h3>See also: <a href='$indexUrl.html'>hub access</a>,&nbsp;<a href='$asmStats.html'>assembly statistics</a></h3><br>

<h3>Data resource links</h3>
NOTE: <em>Click on the column headers to sort the table by that column</em><br>
The <em>link to genome browser</em> will attach only that single assembly to
the genome browser.<br>
The numbers are: item count (percent coverage)<br>
Except for the gc5Base column which is: overall GC % average (percent coverage)
END
}	#	sub startHtml()

##############################################################################
# buildTrackList($testOutput, $asmHubName): the order of columns in the
# table, for either the production ($testOutput = 0) or -test ($testOutput
# = 1) variant.  This used to be a single global array that tableContents()
# mutated in place with these same splices, gated on a global $testOutput --
# calling this twice (once per variant) instead reproduces both variants
# exactly, without needing two separate runs of the script.
##############################################################################
sub buildTrackList($$) {
  my ($testOutput, $asmHubName) = @_;
  # eliminated the ncbiGene track
  my @list = qw(ncbiRefSeq xenoRefGene augustus ensGene gc5Base allGaps assembly rmsk simpleRepeat windowMasker cpgIslandExtUnmasked);
  if ($testOutput) {  # add extra columns during 'test' output
#                       0          1          2        3       4      5    6
#  7       8      9            10           11        12          13
#     14
# my @trackList = qw(ncbiRefSeq xenoRefGene augustus ensGene gc5Base gap allGaps assembly rmsk simpleRepeat windowMasker gapOverlap tandemDups cpgIslandExtUnmasked cpgIslandExt);
#                       0            1         2        3       4      5      6
#      7      8           9               10
# my @trackList = qw(ncbiRefSeq xenoRefGene augustus ensGene gc5Base allGaps assembly rmsk simpleRepeat windowMasker cpgIslandExtUnmasked);
    splice @list, 11, 0, "cpgIslandExt";
    splice @list, 10, 0, "tandemDups";
    splice @list, 10, 0, "gapOverlap";
    splice @list, 5, 0, "gap";
  }
  if ("viral" eq $asmHubName) {
    splice @list, 3, 1;
    splice @list, 2, 1;
    splice @list, 1, 1;
  }
  if ($testOutput || ("viral" eq $asmHubName)) {  # add extra columns during 'test' output
    splice @list, 1, 0, "ncbiGene";
  }
  if ("viral" eq $asmHubName) {
    splice @list, 0, 1;
  }
  return @list;
}

##############################################################################
### start the table output
##############################################################################
sub startTable($) {
  my ($testOutput) = @_;

# coordinate the order of these column headings with buildTrackList() above

print '<table class="sortable" border="1">
<thead style="position:sticky; top:0;"><tr><th>count</th>
  <th>common name<br>link&nbsp;to&nbsp;genome&nbsp;browser</th>
';
  print '<th class="sorttable_numeric">ncbiRefSeq</th>
' if ("viral" ne $asmHubName);

print "  <th class=\"sorttable_numeric\">ncbiGene</th>\n" if ($testOutput || ("viral" eq $asmHubName));

print '  <th class="sorttable_numeric">xenoRefGene</th>
  <th class="sorttable_numeric">augustus<br>genes</th>
  <th class="sorttable_numeric">Ensembl<br>genes</th>
' if ("viral" ne $asmHubName);

print '  <th class="sorttable_numeric">gc5 base</th>
';

if ($testOutput) {
  print "  <th class=\"sorttable_numeric\">AGP<br>gap</th>\n";
  print "  <th class=\"sorttable_numeric\">all<br>gaps</th>\n";
} else {
  print "  <th class=\"sorttable_numeric\">gaps</th>\n";
}

print '  <th class="sorttable_numeric">assembly<br>sequences</th>
  <th class="sorttable_numeric">Repeat<br>Masker</th>
  <th class="sorttable_numeric">TRF<br>simpleRepeat</th>
  <th class="sorttable_numeric">window<br>Masker</th>
';

if ($testOutput) {
print '  <th class="sorttable_numeric">gap<br>Overlap</th>
  <th class="sorttable_numeric">tandem<br>Dups</th>
  <th class="sorttable_numeric">cpg<br>unmasked</th>
  <th class="sorttable_numeric">cpg<br>island</th>
';

} else {
  print "  <th class=\"sorttable_numeric\">cpg<br>islands</th>\n";
}

print "</tr></thead><tbody>\n";
}	#	sub startTable($)

##############################################################################
### end the table output
##############################################################################
sub endTable($$$) {
  my ($assemblyTotal, $asmCount, $columnCount) = @_;

my $percentDone = 100.0 * $asmCount / $assemblyTotal;
my $doneMsg = "";
if ($asmCount < $assemblyTotal) {
  $doneMsg = sprintf(" (%d build completed, %.2f %% finished)", $asmCount, $percentDone);
}
my $colSpanFill = $columnCount - 1;

if ($assemblyTotal > 1) {
  print <<"END"

</tbody>
<tfoot><tr><th>TOTALS:</th><td style='text-align: center;' colspan=$colSpanFill>total assembly count&nbsp;${assemblyTotal}${doneMsg}</td>
  </tr></tfoot>
</table>
END
} else {
  print <<"END"

</tbody>
</table>
END
}
}	#	sub endTable($$$)

##############################################################################
### end the HTML output -- identical for the prod and -test pages
##############################################################################
sub endHtml() {

&commonHtml::otherHubLinks($vgpIndex, $asmHubName);
&commonHtml::htmlFooter($vgpIndex, $asmHubName);

}	#	sub endHtml()

sub asmCounts($) {
  my ($chromSizes) = @_;
  my ($sequenceCount, $totalSize) = split('\s+', `/cluster/bin/x86_64/ave -col=2 $chromSizes | egrep "^count|^total" | awk '{printf "%d\\n", \$NF}' | xargs echo`);
  return ($sequenceCount, $totalSize);
}

sub maskStats($) {
  my ($faSizeFile) = @_;
  my $sizeNoGaps = `grep 'sequences in 1 file' $faSizeFile | awk '{print \$4}'`;
  my $gapSize = `grep 'sequences in 1 file' $faSizeFile | awk '{print \$3}'`;
  chomp $gapSize;
  $gapSize =~ s/\(//;
  my $totalBases = `grep 'sequences in 1 file' $faSizeFile | awk '{print \$1}'`;
  chomp $totalBases;
  my $maskedBases = `grep 'sequences in 1 file' $faSizeFile | awk '{print \$9}'`;
  chomp $maskedBases;
  my $maskPerCent = 100.0 * $maskedBases / $totalBases;
  return ($gapSize, $maskPerCent, $sizeNoGaps);
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
       $gapCount = `awk '{print \$3-\$2}' $gapBed | /cluster/bin/x86_64/ave stdin | grep '^count' | awk '{print \$2}'`;
     }
  } elsif ( -s "$gapBed" ) {
    $gapCount = `zcat $gapBed | awk '{print \$3-\$2}' | /cluster/bin/x86_64/ave stdin | grep '^count' | awk '{print \$2}'`;
  }
  chomp $gapCount;
  return ($gapCount);
}

##############################################################################
# computeTrackCell($asmId, $track, $buildDir, $totalSize)
# returns (itemCount, percentCover, customKey) for one track of one
# assembly.  This is the expensive part (bigWigInfo/bigBedInfo/hgsql/etc.)
# and is testOutput-independent, so it only needs to run once per assembly
# per track no matter how many page variants reference that track.
#
# NOTE: the original script additionally retried a "still n/a" ensGene or
# ncbiRefSeq track as ebiGene/ncbiGene, but on production output only. That
# retry re-checked the *same* file path that had just been found missing
# (only $runDir and the diagnostic track-name argument differed, and
# oneTrackData() only consults $runDir for the unrelated 'gapOverlap' case)
# -- so it was a guaranteed no-op and is not reproduced here.
##############################################################################
sub computeTrackCell($$$$) {
  my ($asmId, $track, $buildDir, $totalSize) = @_;
  my $trackFile = "$buildDir/bbi/$asmId.$track";
  my $trackFb = "$buildDir/trackData/$track/fb.$asmId.$track.txt";
  # no ensGene file ?  Then look for ebiGene file
  if ($track eq "ensGene" && ! -s $trackFb) {
    if ( -d "$buildDir/trackData/ebiGene" ) {
      $trackFb = "$buildDir/trackData/ebiGene/fb.ebiGene.txt" if ( -d "$buildDir/trackData/ebiGene/fb.ebiGene.txt");
      $trackFile = "$buildDir/bbi/$asmId.ebiGene";
    }
  }
  my $runDir = "$buildDir/trackData/$track";
  my ($itemCount, $percentCover);
  my $customKey = "";
  if ($asmId !~ m/^GC/) {
    $itemCount = "n/a";
    $percentCover = "n/a";
    if ($track eq "ncbiRefSeq") {
      my $refSeqDir=`ls -d /hive/data/genomes/$asmId/bed/ncbiRefSeq.20* | tail -1`;
      chomp $refSeqDir;
      if ( -d "${refSeqDir}" ) {
        my $trackFb = "${refSeqDir}/fb.ncbiRefSeq.$asmId.txt";
        if ( -s "${trackFb}" ) {
          $itemCount = `hgsql -N -e 'select count(*) from $track;' $asmId 2> /dev/null`;
          chomp $itemCount;
          $percentCover = pcFbFile($trackFb);
        }
      }
    } elsif ($track eq "gc5Base") {
      my $bwFile = "/gbdb/$asmId/bbi/gc5Base.bw";
      $bwFile = "/gbdb/$asmId/bbi/gc5BaseBw/gc5Base.bw" if (! -s "${bwFile}");
      ($itemCount, $percentCover) = bigWigMeasure($bwFile, $totalSize);
    } elsif ($track eq "rmsk") {
      my $rmskStats = "/hive/data/genomes/$asmId/bed/repeatMasker/$asmId.rmsk.stats";
      if (! -s "${rmskStats}") {
        my $faOut = "/hive/data/genomes/$asmId/bed/repeatMasker/$asmId.sorted.fa.out.gz";
        if ( -s "$faOut") {
	    my $items = `zgrep -c ^ "$faOut"`;
	    chomp $items;
	    $itemCount = commify($items);
	    my $masked = `grep masked "/hive/data/genomes/$asmId/bed/repeatMasker/faSize.rmsk.txt" | awk '{print \$4}' | sed -e 's/%//;'`;
	    chomp $masked;
	    $percentCover = sprintf("%.2f %%", $masked);
	    open (RS, ">$rmskStats") or die "can now write to $rmskStats";
	    printf RS "%s\t%s\n", $itemCount, $percentCover;
	    close (RS);
	} else {
	    $itemCount = "n/a";
	    $percentCover = "n/a";
        }
      } else {
        ($itemCount, $percentCover) = split('\s+', `cat $rmskStats`);
        chomp $percentCover;
        $customKey = sprintf("%.2f", $percentCover);
        $percentCover = sprintf("%.2f %%", $percentCover);
      }
    }	# elsif ($track eq "rmsk")
  } else {	# working on an assembly hub
    if ( "$track" eq "gc5Base" ) {
      $trackFile .= ".bw";
    } else {
      $trackFile .= ".bb";
    }
    if ( "$track" eq "rmsk") {
      my $rmskStats = "$buildDir/trackData/repeatMasker/$asmId.rmsk.stats";
      if (! -s "${rmskStats}") {
        my $faOut = "$buildDir/trackData/repeatMasker/$asmId.sorted.fa.out.gz";
        if ( -s "$faOut") {
            my $items = `zgrep -c ^ "$faOut"`;
            chomp $items;
            $itemCount = commify($items);
            my $masked = `grep masked "$buildDir/trackData/repeatMasker/faSize.rmsk.txt" | awk '{print \$4}' | sed -e 's/%//;'`;
            chomp $masked;
            $percentCover = sprintf("%.2f %%", $masked);
            open (RS, ">$rmskStats") or die "can now write to $rmskStats";
            printf RS "%s\t%s\n", $itemCount, $percentCover;
            close (RS);
        } else {
            $itemCount = "n/a";
            $percentCover = "n/a";
        }
      } else {
        ($itemCount, $percentCover) = split('\s+', `cat $rmskStats`);
        chomp $percentCover;
        $customKey = sprintf("%.2f", $percentCover);
        $percentCover = sprintf("%.2f %%", $percentCover);
      }
    } else {	# not the rmsk track
      ($itemCount, $percentCover) = oneTrackData($asmId, $track, $trackFile, $totalSize, $trackFb, $runDir);
    }	#       else not the rmsk track
  }		#       else if ($asmId !~ m/^GC/)
  if (($percentCover =~ m/%/) || ($percentCover !~ m#n/a#)) {
    $customKey = $percentCover;
    $customKey =~ s/[ %]+//;
  }
  return ($itemCount, $percentCover, $customKey);
}	#	sub computeTrackCell($$$$)

# render one <td> cell from a (itemCount, percentCover, customKey) triple
sub renderCell($$$) {
  my ($itemCount, $percentCover, $customKey) = @_;
  if (length($customKey)) {
    return sprintf("    <td style='text-align: right;' sorttable_customkey='%s'>%s<br>(%s)</td>\n", $customKey, $itemCount, $percentCover);
  } elsif ($itemCount eq "n/a") {
    return "    <td style='text-align: right;'>n/a</td>\n";
  } else {
    return sprintf("    <td style='text-align: right;'>%s<br>(%s)</td>\n", $itemCount, $percentCover);
  }
}

##############################################################################
### tableContentsBoth()
### walks @orderList exactly once, measuring each assembly's tracks exactly
### once, and returns the table body HTML for both the production and
### -test pages (plus each page's column count, for endTable()'s colspan).
##############################################################################
sub tableContentsBoth() {
  my @prodTrackList = buildTrackList(0, $asmHubName);
  my @testTrackList = buildTrackList(1, $asmHubName);
  my %inUnion;
  my @unionTracks = grep { !$inUnion{$_}++ } (@prodTrackList, @testTrackList);

  my $prodBody = "";
  my $testBody = "";
  my $asmCounted = 0;

  foreach my $asmId (@orderList) {
    my $gcPrefix = "GCx";
    my $asmAcc = "asmAcc";
    my $asmName = "asmName";
    my $accessionId = "GCx_098765432.1";
    my $accessionDir = "";
    my $configRa = "n/a";
    my $tracksCounted = 0;
    my $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir/$asmId";
    my $asmReport="$buildDir/download/${asmId}_assembly_report.txt";
    my $chromSizes = "${buildDir}/${asmId}.chrom.sizes";
    my $twoBit = "${buildDir}/trackData/addMask/${asmId}.masked.2bit";
    my $faSizeTxt = "${buildDir}/${asmId}.faSize.txt";
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
       $buildDir="/hive/data/outside/ncbi/genomes/$accessionDir/${accessionId}_${asmName}";
       $asmReport="$buildDir/${accessionId}_${asmName}_assembly_report.txt";
       $chromSizes = "/hive/data/genomes/$asmId/chrom.sizes";
       $twoBit = "/hive/data/genomes/$asmId/$asmId.2bit";
       $faSizeTxt = "/hive/data/genomes/$asmId/faSize.${asmId}.2bit.txt";
    } else {
       ($gcPrefix, $asmAcc, $asmName) = split('_', $asmId, 3);
       $accessionId = sprintf("%s_%s", $gcPrefix, $asmAcc);
       $accessionDir = substr($asmId, 0 ,3);
       $accessionDir .= "/" . substr($asmId, 4 ,3);
       $accessionDir .= "/" . substr($asmId, 7 ,3);
       $accessionDir .= "/" . substr($asmId, 10 ,3);
      $buildDir = "/hive/data/genomes/asmHubs/refseqBuild/$accessionDir/$asmId";
       if ($gcPrefix eq "GCA") {
     $buildDir = "/hive/data/genomes/asmHubs/genbankBuild/$accessionDir/$asmId";
       }
       $asmReport="$buildDir/download/${asmId}_assembly_report.txt";
       $chromSizes = "${buildDir}/${asmId}.chrom.sizes";
       $twoBit = "${buildDir}/trackData/addMask/${asmId}.masked.2bit";
       $faSizeTxt = "${buildDir}/${asmId}.faSize.txt";
    }
    if (! -s "$asmReport") {
      printf STDERR "# no assembly report:\n# %s\n", $asmReport;
      next;
    }
    if (! -s "$twoBit") {
      printf STDERR "# no 2bit file:\n# %s\n", $twoBit;
      my $missingRow = sprintf("<tr><td style='text-align: right;'>%d</td>\n", ++$asmCount);
      $missingRow .= sprintf("<td style='text-align: center;'>%s</td>\n", $accessionId);
      $missingRow .= "<th colspan=15 style='text-align: center;'>missing masked 2bit file</th>\n";
      $missingRow .= "</tr>\n";
      $prodBody .= $missingRow;
      $testBody .= $missingRow;
      next;
    }
    if ( ! -s "$faSizeTxt" ) {
       printf STDERR "faSize $twoBit > $faSizeTxt\n";
       print `faSize $twoBit > $faSizeTxt`;
    }
    my ($gapSize, $maskPerCent, $sizeNoGaps) = maskStats($faSizeTxt);
    $overallGapSize += $gapSize;
    my ($seqCount, $totalSize) = asmCounts($chromSizes);
    $overallSeqCount += $seqCount;
    $overallNucleotides += $totalSize;
    my $gapCount = gapStats($buildDir, $asmId);
    $overallGapCount += $gapCount;
    my $sciName = "notFound";
    $sciName = $sciNameOverride{$accessionId} if (defined($sciNameOverride{$accessionId}));
    my $commonName = "notFound";
    my $asmDate = "notFound";
    my $itemsFound = 0;
    open (FH, "<$asmReport") or die "can not read $asmReport";
    while (my $line = <FH>) {
      last if ($itemsFound > 5);
      chomp $line;
      $line =~ s///g;;
      $line =~ s/\s+$//g;;
      if ($line =~ m/Date:/) {
        if ($asmDate =~ m/notFound/) {
           ++$itemsFound;
           $asmDate = $line;
           $asmDate =~ s/.*:\s+//;
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
      }
    }
    close (FH);

    # the browser/hub links are the only thing that differ between the
    # two pages besides the columns themselves
    my $browserName = $commonName;
    my $prodBrowserUrl = "https://genome.ucsc.edu/h/$accessionId";
    my $testBrowserUrl = "https://genome-test.gi.ucsc.edu/h/$accessionId";
    if ($asmId !~ m/^GC/) {
       $prodBrowserUrl = "https://genome.ucsc.edu/cgi-bin/hgTracks?db=$asmId";
       $testBrowserUrl = "https://genome-test.gi.ucsc.edu/cgi-bin/hgTracks?db=$asmId";
       $browserName = "$commonName ($asmId)";
    }

    ++$asmCount;
    my $prodRow = sprintf("<tr><td style='text-align: right;'>%d</td>\n", $asmCount);
    $prodRow .= sprintf("<td style='text-align: center;'><a href='%s' target=_blank>%s<br>%s</a></td>\n", $prodBrowserUrl, $browserName, $accessionId);
    my $testRow = sprintf("<tr><td style='text-align: right;'>%d</td>\n", $asmCount);
    $testRow .= sprintf("<td style='text-align: center;'><a href='%s' target=_blank>%s<br>%s</a></td>\n", $testBrowserUrl, $browserName, $accessionId);

    # measure every track needed by either page exactly once
    my %cell;	# key is track name, value is [itemCount, percentCover, customKey]
    foreach my $track (@unionTracks) {
      $cell{$track} = [ computeTrackCell($asmId, $track, $buildDir, $totalSize) ];
    }

    foreach my $track (@prodTrackList) {
      my ($itemCount, $percentCover, $customKey) = @{$cell{$track}};
      $tracksCounted += 1 if ($itemCount ne "n/a");
      $prodRow .= renderCell($itemCount, $percentCover, $customKey);
    }
    foreach my $track (@testTrackList) {
      my ($itemCount, $percentCover, $customKey) = @{$cell{$track}};
      $testRow .= renderCell($itemCount, $percentCover, $customKey);
    }
    $prodRow .= "</tr>\n";
    $testRow .= "</tr>\n";
    $prodBody .= $prodRow;
    $testBody .= $testRow;

    $asmCounted += 1;
    if ($asmId =~ m/^GC/) {
       printf STDERR "# %03d\t%02d tracks\t%s\n", $asmCounted, $tracksCounted, $asmId;
    } else {
       printf STDERR "# %03d\t%02d tracks\t%s_%s (%s)\n", $asmCounted, $tracksCounted, $accessionId, $asmName, $asmId;
    }
  }
  return ($prodBody, $testBody, scalar(@prodTrackList), scalar(@testTrackList));
}	#	sub tableContentsBoth()

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

my $header = capture { startHtml() };
my ($prodBody, $testBody, $prodCols, $testCols) = tableContentsBoth();
my $prodHead = capture { startTable(0) };
my $testHead = capture { startTable(1) };
my $prodFoot = capture { endTable($assemblyTotal, $asmCount, $prodCols) };
my $testFoot = capture { endTable($assemblyTotal, $asmCount, $testCols) };
my $footer = capture { endHtml() };

open(my $pfh, '>', $prodOutFile) or die "can not write $prodOutFile";
print $pfh $header, $prodHead, $prodBody, $prodFoot, $footer;
close($pfh);

open(my $tfh, '>', $testOutFile) or die "can not write $testOutFile";
print $tfh $header, $testHead, $testBody, $testFoot, $footer;
close($tfh);
