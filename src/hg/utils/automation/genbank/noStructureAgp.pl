#!/bin/env perl

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
  my ($seqName, $seqRole, $assignedMolecule, $moleculeType, $genbankAcc, $relationship, $refseqAcc, $asmUnit) = split('\s+', $line);
  die "can not find $genbankAcc in $chrSizes" if (!exists($chrSizes{$genbankAcc}));
  my $size = $chrSizes{$genbankAcc};
## XXX trying without the v name translation
##  $seqName =~ s/\.\([0-9]+\)/v$1/;
  my $partName = $seqName;  # assume ucsc naming first
  $partName = $genbankAcc if ($ncbiUcsc eq "ncbi");
  my $chrPrefix = "chr";
  $chrPrefix = "" if ($ncbiUcsc eq "ncbi");
  if (($seqRole =~ m/assembled-molecule/) && ($moleculeType =~ m/Chromosome/)) {
    printf "%s%s\t1\t%d\t%d\tF\t%s\t1\t%d\t+\n", $chrPrefix, $partName, $size, ++$partNum, $genbankAcc, $size;
  } else {
    printf "%s\t1\t%d\t%d\tF\t%s\t1\t%d\t+\n", $partName, $size, ++$partNum, $genbankAcc, $size;
  }
}
close (FH);
