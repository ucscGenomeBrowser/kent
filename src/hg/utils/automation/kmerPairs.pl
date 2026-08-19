#!/usr/bin/env perl

use strict;
use warnings;

# 2026-08-19 - performance improvement from previous version
#            - claude diagnosed inefficiet processing, straightened it up
#            - changes tested and proved to be byte for byte identical output
#            - about a 4X speed improvement
#
# The original kmerPairs.pl made two passes over the duplicated kmers:
# it first read the whole sorted input into a %dups hash of kmer =>
# array of start positions, then did 'sort keys (%dups)' to recover
# an order (lost by going through a hash) before comparing every pair
# of positions for each kmer with a full O(m^2) all-pairs scan (m =
# number of times that kmer occurs).  For a kmer that recurs heavily
# (satellite/centromeric repeat, high-copy transposon) m can be in the
# thousands, making that inner loop the dominant cost of the job.
#
# This version makes a single streaming pass over the already-sorted
# input (sorted by kmer, then by start -- same 'sort -k1,1 -k3,3n' as
# before) and, per kmer group, keeps only a sliding window of starts
# that are still within $gapSize of the current position.  Positions
# only increase within a group, so once a start falls $gapSize or
# more behind the current one it can never satisfy the gap test
# again for any later position either -- it is dropped from the
# window for good.  That turns the per-kmer cost from O(m^2) into
# O(m + pairs emitted), and it removes the need for the %dups hash
# and the second 'sort keys' pass entirely.

my $argc = scalar(@ARGV);
if ($argc != 4) {
  printf STDERR "usage: kmerPairs.pl kmerSize gapSize chrName tmp/chrName.bed.gz\n";
  printf STDERR "pairs up identical kmers of size kmerSize with gaps from 1 to gapSize\n";
  printf STDERR "the output for each pair is a two exon bed item,\n";
  printf STDERR "the two exons are the locations of the kmers, and the score\n";
  printf STDERR "column is the size of the kmer (== size of exon)\n";
  printf STDERR "and the intron is the size of the gap between the kmers\n";
  printf STDERR "Expects to find kmer source file: ../kmers/tmp/chrName.{kmerSize}mer.txt.gz\n";
  printf STDERR "which is three columns: kmerString chrName chrStart\n";
  printf STDERR "Will be writing bed output to given result file: tmp/chrName.bed.gz\n";
  printf STDERR "Expects the tmp/ directory to already exist.\n";
  exit 255;
}

my $kmerSize = shift;
my $gapSize = shift;
my $chrName = shift;
my $output = shift;
my $srcKmers = "../kmers/tmp/$chrName.${kmerSize}mer.txt.gz";

open (FH, "zcat $srcKmers | sort -k1,1 -k3,3n|") or die "can not read $chrName.${kmerSize}mer.txt.gz";
open (OT, "|gzip -c > $output") or die "can not gzip to $output";

my $prevKmer = "";
my @starts;	# sliding window of start positions for the current kmer group
my $lo = 0;	# index of oldest start in @starts still within $gapSize

while (my $line = <FH>) {
  chomp $line;
  my ($kmer, undef, $start) = split('\s+', $line);

  if ($kmer ne $prevKmer) {
    # new kmer group begins; start a fresh window
    @starts = ();
    $lo = 0;
    $prevKmer = $kmer;
  }

  # drop starts that are now $gapSize or more behind the current one;
  # since starts only increase from here on, they can never come back
  # into the window
  while ($lo < scalar(@starts) && ($start - $starts[$lo]) >= $gapSize) {
    ++$lo;
  }

  for (my $j = $lo; $j < scalar(@starts); ++$j) {
    my $distance = $start - $starts[$j];
    if ($distance > $kmerSize) {
      my $txStart = $starts[$j];
      my $txEnd = $start + $kmerSize;
      printf OT "%s\t%d\t%d\t%s:%d-%d\t%d\t+\t%d\t%d\t0\t2\t%d,%d\t0,%d\n", $chrName, $txStart, $txEnd, $chrName, $txStart+1, $txEnd, $kmerSize, $txStart, $txEnd, $kmerSize, $kmerSize, $distance;
    }
  }

  push @starts, $start;
}
close (FH);
close (OT);
