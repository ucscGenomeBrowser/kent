#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./noStructureAgp.pl [ucsc|ncbi] <pathTo>/*_assembly_report.txt <pathTo>/*.ncbi.chrom.sizes\n";
  printf STDERR "output AGP is to stdout, redirect to > noStructure.agp\n";
  printf STDERR "the ucsc|ncbi selection decides which name to use\n";
}

my $argc = scalar(@ARGV);

if ($argc < 3) {
  usage;
  exit 255;
}

my $ncbiUcsc = shift(@ARGV);
my $asmReport = shift(@ARGV);
my $chrSizes = shift(@ARGV);

my %chrSizes;  # key is NCBI accession name, value is size of contig/chrom

open (FH, "<$chrSizes") or die "can not read $chrSizes";
while (my $line = <FH>) {
  chomp $line;
  my ($accession, $size) = split('\s+', $line);
  $chrSizes{$accession} = $size;
}
close (FH);

my $partNum = 0;
open (FH, "<$asmReport") or die "can not read $asmReport";
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp $line;
  my $size = 0;
  my $ncbiName = "";
  my ($seqName, $seqRole, $assignedMolecule, $moleculeType, $genbankAcc, $relationship, $refseqAcc, $asmUnit, $seqLength, $ucscName) = split('\t', $line);
  if (!exists($chrSizes{$genbankAcc})) {
     die "can not find $genbankAcc or $refseqAcc in $chrSizes" if (!exists($chrSizes{$refseqAcc}));
     $ncbiName = $refseqAcc;
  } else {
     $ncbiName = $genbankAcc;
  }
  $size = $chrSizes{$ncbiName};
  my $partName = $seqName;  # assume ucsc naming first
  $partName = $ncbiName if ($ncbiUcsc eq "ncbi");
  my $chrPrefix = "chr";
  $chrPrefix = "" if ($ncbiUcsc eq "ncbi");
  if (($seqRole =~ m/assembled-molecule/) && ($moleculeType =~ m/Chromosome/)) {
    printf "%s%s\t1\t%d\t%d\tF\t%s\t1\t%d\t+\n", $chrPrefix, $partName, $size, ++$partNum, $ncbiName, $size;
  } else {
    printf "%s\t1\t%d\t%d\tF\t%s\t1\t%d\t+\n", $partName, $size, ++$partNum, $genbankAcc, $size;
  }
}
close (FH);
