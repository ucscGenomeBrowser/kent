#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc < 2) {
  printf STDERR "usage: kmerPrint.pl <N> <fastaFile.fa> [moreFastaFiles.fa] | gzip -c > result.gz\n";
  printf STDERR "where <N> is kmer size\n";
  printf STDERR "followed by fasta file names (can be 'stdin')\n";
  exit 255;
}

# first argument is the id to place on each kmer
# first argument is kmer size
my $kmerSize = shift;

my $chrName = "";
my $chrStart = 0;

#######################################################################
sub countKmers($) {
  my ($sequence) = @_;
  my $sequenceLen = length($sequence);
  for (my $i = 0; $i <= ($sequenceLen - $kmerSize); ++$i) {
    my $kmer = substr($sequence, $i, $kmerSize);
    printf "%s\t%s\t%d\n", $kmer, $chrName, $chrStart if($kmer !~ /[^ACGT]/);
    ++$chrStart;
  }
}

##########################################################################

# read in the fasta files
while (my $file = shift) {
  my $sequence = "";
  if ($file =~ m/stdin/) {
    open (FH, "</dev/stdin") or die "can not read stdin";
  } elsif ($file =~ m/.gz$/) {
    open (FH, "gzcat $file|") or die "can not gzcat $file";
  } else {
    open (FH, "<$file") or die "can not read $file";
  }
  while (my $line = <FH>) {
    # skip comment lines
    next if ($line =~ m/^\s*#/);
    # skip empty lines
    next if ($line =~ m/^\s*$/);
    chomp $line;
    if ($line =~ m/^>/) {
      if (length($sequence)) {
        countKmers($sequence);
        $sequence = "";
      }
      chomp $line;
      $chrName = $line;
      $chrName =~ s/^>//;
      $chrStart = 0;
    } else {
      $sequence .= uc($line);
    }
  }
  if (length($sequence)) {
     countKmers($sequence);
     $sequence = "";
  }
  close (FH);
}
