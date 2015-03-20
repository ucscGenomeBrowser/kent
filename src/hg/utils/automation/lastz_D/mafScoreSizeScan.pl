#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 1) {
  printf STDERR "usage: ./mafScoreSizeScan.pl <file.oneOff.maf> > mafScoreSizeScan.list\n";
  printf STDERR "outputs to stdout, tab separated fields:\n";
  printf STDERR "score size target query tStrand qStrand tProp qProp coverage\n";
  printf STDERR "score - maf score\n";
  printf STDERR "size - length of aligned block\n";
  printf STDERR "target - target chrom/fragment name\n";
  printf STDERR "query - query chrom/fragment name\n";
  printf STDERR "tStrand, qStrand - strand of target and query for the alignment\n";
  printf STDERR "tProp, qProp - percent of size covered by this target/query block\n";
  printf STDERR "coverage = 100.0 * qProp / tProp\n";
  exit 255;
}

my $mafFile = shift;
open (FH, "<$mafFile") or die "can not read $mafFile";
my ($score, $target, $query, $nameCount, $length) = (0, "", "", 0, 0);
my ($tStrand, $qStrand, $tSequence, $qSequence) = ("", "", "", "");
my ($tProportion, $qProportion) = (0, 0);
printf "# score\tsize\ttarget\tquery\ttStrand\tqStrand\ttProp\tqProp\tcoverage\n";
while (my $line = <FH>) {
  chomp $line;
  if ($line =~ m/^a score=/) {
    $score = $line;
    $score =~ s/a score=//;
    $score =~ s/.000000//;
    $length = 0; $nameCount = 0;
    $tStrand = "."; $qStrand = ".";
    $tSequence = ""; $qSequence = "";
  } elsif ($line =~ m/^s /) {
    my (undef, $name, $offset, $size, $strand, $totalLength, $sequence) =
       split('\s+', $line);
    if ( 0 == $nameCount) {
      $target = $name;
      $length = $size;
      $tStrand = $strand;
      $tSequence = $sequence;
      $tProportion = 100.0 * $size / $totalLength;
    } else {
      $query = $name;
      $qStrand = $strand;
      $qSequence = $sequence;
      $qProportion = 100.0 * $size / $totalLength;
    }
    ++$nameCount;
    if (2 == $nameCount) {
       my $coverage = 100.0 * $qProportion / $tProportion;
       printf "%d\t%d\t%s\t%s\t%s\t%s\t%.2f\t%.2f\t%.2f\n",
           $score, $length, $target, $query, $tStrand, $qStrand, $tProportion, $qProportion, $coverage;
    }
  }
}
close (FH);
