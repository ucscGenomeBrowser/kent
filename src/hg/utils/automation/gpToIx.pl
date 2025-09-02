#!/usr/bin/env perl

# construct ix.txt index from a genePred file
# index key is the name of the gene from column 1 of the genePred
# alias names for that are constructed:
#   the name without any .nnn suffix
#   name2 if present, and name2 without .nnn suffix

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1 ) {
  printf STDERR "usage: gpToIx.pl <genePred.gp> | sort -u > ix.txt\n";
  printf STDERR "then run ixIxx on ix.txt:\n";
  printf STDERR " ixIxx ix.txt out.ix out.ixx\n";
  exit 255;
}

my $gpFile = shift;

if ($gpFile =~ m/.gz$/) {
  open (FH, "zcat $gpFile|") or die "ERROR: gpToIx.pl can not read '$gpFile'";
} else {
  open (FH, "<$gpFile") or die "ERROR: gpToIx.pl can not read '$gpFile'";
}
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp ($line);
  my ($itemName, $chrom, $strand, $txStart, $txEnd, $cdsStart, $cdsEnd, $exonCount, $exonStarts, $exonEnds, $score, $name2, $cdsStartStat, $cdsEndStat, $exonFrames) = split('\s+', $line);
  my $name = $itemName;
  my $extraNames = "";
  my $ncbiName = "";
  my $isNcbiName = 0;
  $isNcbiName = 1 if ($name =~ m/\|/);
  if ($isNcbiName) {
    my @ncbiParts = split('\|', $name);
    if (defined($ncbiParts[2])) {
      $name = $ncbiParts[2];
      $name =~ s/^mrna.//;
      $extraNames = $name;
    }
  }
  my $noSuffix=$name;
  $noSuffix =~ s/\.[0-9][0-9]*$//;
  if (($noSuffix ne $name) && (length($noSuffix) > 0)) {
      if (length($extraNames) > 0) {
         $extraNames .= "\t" . $noSuffix;
      } else {
         $extraNames = $noSuffix;
      }
  }
  if (defined($name2)) {
    if (lc($name) ne lc($name2)) {
      if (length($extraNames) > 0) {
         $extraNames .= "\t" . $name2;
      } else {
         $extraNames = $name2;
      }
      $noSuffix = $name2;
      $noSuffix =~ s/\.[0-9][0-9]*$//;
      $extraNames .= "\t" . $noSuffix if (($noSuffix ne $name2) && (length($noSuffix) > 0));
    }
  }
  printf "%s\t%s\n", $itemName, $extraNames if (length($extraNames) > 0);
}
close (FH);
