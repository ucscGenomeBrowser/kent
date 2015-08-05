#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/categorizeSnps.pl instead.

use warnings;
use strict;

# This is the minimum number of "chromosomes" on which an allele has been reported.
# Unfortunately some variants have been repeatedly submitted 2 or 3 times by the same
# submitters, so it's hard to believe an allele frequency report with low N.
my $minTotal2N = 10;

my %rc = ( 'A' => 'T',
	   'C' => 'G',
	   'G' => 'C',
	   'T' => 'A',
	   'N' => 'N',
	 );

sub revComp($) {
  # Reverse-complement a nucleotide sequence, possible containing N's.
  my ($seq) = @_;
  die "How do I revComp '$seq'?" unless ($seq =~ /^[ACGTN]+$/);
  my $rcSeq = "";
  while ($seq =~ s/(.)$//) {
    $rcSeq .= $rc{$1};
  }
  return $rcSeq;
} # revComp


# Main: open output files and assign each input file to one of them.

my ($multCount, $comCount, $flagCount, $miscCount) = (0,0,0,0);
open(my $mult,    "| gzip -c > Mult.bed.gz") || die;
open(my $common,  "| gzip -c > Common.bed.gz") || die;
open(my $flagged, "| gzip -c > Flagged.bed.gz") || die;
while (<>) {
  my @w = split("\t");
  # Use only our own MultipleAlignments accounting, not dbSNP's weight; dbSNP may assign
  # a weight of 3 for a variant that maps uniquely to each MHC alt (e.g. b144 rs928).
  if ($w[17] =~ /MultipleAlignments/) {
    print $mult $_;
    $multCount++;
  } else {
    # Parse allele freq columns to find max allele frequency.  Also try to figure
    # out which allele freq is for the reference allele.  If the reference allele
    # freq is 0, we still want to include this variant in Common SNPs even though
    # it doesn't seem to vary; the reference is either wrong or very rare.
    my ($alleleFreqCount, $alStr, $nStr, $freqStr) = ($w[20], $w[21], $w[22], $w[23]);
    my @als = split(",", $alStr);      die unless scalar(@als) == $alleleFreqCount;
    my @alNs = split(",", $nStr);      die unless scalar(@alNs) == $alleleFreqCount;
    my @freqs = split(",", $freqStr);  die unless scalar(@freqs) == $alleleFreqCount;
    # Alleles are on SNP strand, while ref is on + strand; revComp if necessary:
    my ($strand, $refAl) = ($w[5], $w[7]);
    if ($strand eq "-" && $refAl =~ /^[ACGTN]$/) {
      $refAl = revComp($refAl);
    }
    my ($total2N, $maxAlleleFreq, $refFreqIs0) = (0, 0, 0);
    for (my $i = 0;  $i < $alleleFreqCount;  $i++) {
      $total2N += $alNs[$i];
      $maxAlleleFreq = $freqs[$i] if ($freqs[$i] > $maxAlleleFreq);
      if ($als[$i] eq $refAl && $alNs[$i] == 0) {
	$refFreqIs0 = 1;
      }
    }
    if ($alleleFreqCount >= 2 && $total2N >= $minTotal2N &&
	($maxAlleleFreq <= 0.99 || $refFreqIs0)) {
      print $common $_;
      $comCount++;
    } elsif ($w[24] =~ /clinically-assoc/) {
      print $flagged $_;
      $flagCount++;
    } else {
      $miscCount++;
    }
  }
}
close($mult);  close($common); close($flagged);
print "Mult:     $multCount\n"
    . "Common:   $comCount\n"
    . "Flagged:  $flagCount\n"
    . "leftover: $miscCount\n";
