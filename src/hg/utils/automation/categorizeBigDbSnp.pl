#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/categorizeBigDbSnp.pl instead.

use warnings;
use strict;

# Use 1000Genomes frequencies for determining whether a variant is "Common" or not, as dbSNP does.
# By convention, 1000Genomes is always first in freqSourceOrder.
my $freqSourceIndex = 0;

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

my $prefix = shift @ARGV;

die "Expecting at least one argument: prefix [input.bigDbSnp]\n" if (! $prefix);

my ($multCount, $comCount, $clinCount, $clinComCount, $miscCount) = (0,0,0,0);
open(my $mult,    "> $prefix.Mult.bigDbSnp") || die;
open(my $common,  "> $prefix.Common.bigDbSnp") || die;
open(my $clinVar, "> $prefix.ClinVar.bigDbSnp") || die;
while (<>) {
  my @w = split("\t");
  my $ucscNotes = $w[14];
  if ($ucscNotes =~ /multiMap/) {
    print $mult $_;
    $multCount++;
  } else {
    my $isClinVar = ($ucscNotes =~ /clinvar/);
    my $isCommon = 0;
    my @freqs = split(",", $w[9]);
    my $freq = $freqs[$freqSourceIndex];
    if ($freq && $freq !~ /nan/ && $freq !~ /inf/) {
      if ($freq > 0.01 ||
          ($freq == 0.0 && $ucscNotes =~ /refIsSingleton/)) {
        $isCommon = 1;
      }
    }
    if ($isCommon) {
      print $common $_;
      $comCount++;
    }
    if ($isClinVar) {
      print $clinVar $_;
      $clinCount++;
    }
    if ($isCommon && $isClinVar) {
      $clinComCount++;
    }
    if (! ($isCommon || $isClinVar)) {
      $miscCount++;
    }
  }
}
close($mult);  close($common); close($clinVar);
print "Mult:     $multCount\n"
    . "Common:   $comCount\n"
    . "ClinVar:  $clinCount\n"
    . "leftover: $miscCount\n"
    . "Common and ClinVar: $clinComCount\n";
