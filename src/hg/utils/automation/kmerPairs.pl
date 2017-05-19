#!/usr/bin/env perl

use strict;
use warnings;

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

my $prevKmer = "";
my $prevStart = 0;
my %dups;	# key is kmer name, value is pointer to array with start values

open (FH, "zcat $srcKmers | sort -k1,1 -k3,3n|") or die "can not read $chrName.${kmerSize}mer.txt.gz";
while (my $line = <FH>) {
  chomp $line;
  my ($kmer, $chrName, $start) = split('\s+', $line);
  if ($kmer eq $prevKmer) {
    if (! exists($dups{$kmer})) {
       my @a;
       push @a, $prevStart;
       $dups{$kmer} = \@a;
    }
    my $arrayPtr = $dups{$kmer};
    push @$arrayPtr, $start;
  }
  $prevKmer = $kmer;
  $prevStart = $start;
}
close (FH);

open (OT, "|gzip -c > $output") or die "can not gzip to $output";

my $dupCount = 0;
foreach my $kmer (sort keys (%dups)) {
  my $kmerSize = length($kmer);
  my $arrayPtr = $dups{$kmer};
  my @starts;
  my $i = 0;
  foreach my $start (@$arrayPtr) {
     for (my $j = 0; $j < $i; ++$j) {
       my $distance = $start - $starts[$j];
       if ( ($distance > $kmerSize) && ($distance < $gapSize)) {
         my $txStart = $starts[$j];
         my $txEnd = $start + $kmerSize;
         
         printf OT "%s\t%d\t%d\t%s:%d-%d\t%d\t+\t%d\t%d\t0\t2\t%d,%d\t0,%d\n", $chrName, $txStart, $txEnd, $chrName, $txStart+1, $txEnd, $kmerSize, $txStart, $txEnd, $kmerSize, $kmerSize, $distance;
       }
     }
     $starts[$i++] = $start;
  }
}

close (OT);
