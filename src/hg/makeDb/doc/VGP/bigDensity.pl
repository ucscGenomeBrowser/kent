#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 3) {
  printf STDERR "usage: bigDensity.pl <windowSize> <chromSizes> <file.genePred>\n";
  exit 255
}

my $winSize = shift;
my $sizesFile = shift;
my $genePred = shift;

my %chromSizes;	# key is chr name, value is size

open (FH, "<$sizesFile") or die "can not read $sizesFile";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $size) = split('\s+', $line);
  $chromSizes{$chr} = $size;
}
close (FH);

my %windows;  # key is chr name, value is pointer to array of counts per window
#### initialize all window counts to zero
foreach my $chr (keys %chromSizes) {
  my @a;
  my $arrPtr = \@a;
  $windows{$chr} = $arrPtr;
  my $size = $chromSizes{$chr};
  my $j = 0;
  for (my $i = 0; $i < $size; $i += $winSize) {
     $arrPtr->[$j++] = 0;
  }
#  printf STDERR "# %s %d %d\n", $chr, $j, $size;
}

if ($genePred =~ m/.gz/) {
  open (FH, "zcat $genePred|") or die "can not zcat $genePred";
} else {
  open (FH, "< $genePred|") or die "can not read $genePred";
}

my $chrPtr;
my $thisChr = "";
while (my $line = <FH>) {
  chomp $line;
  my ($name, $chr, $strand, $txStart, $txEnd, undef) = split('\s+', $line, 6);
  if (length($thisChr)) {
    if ($chr ne $thisChr) {
    $thisChr = $chr;
    $chrPtr = $windows{$chr};
    }
  } else {
    $thisChr = $chr;
    $chrPtr = $windows{$chr};
  }
  my $windowS = int ($txStart / $winSize);
  $chrPtr->[$windowS] += 1;
  my $windowE = int ($txEnd / $winSize);
  if ($windowE != $windowS) {
     $chrPtr->[$windowE] += 1;
     for (my $i = $windowS + 1; $i < $windowE; ++$i) {
       $chrPtr->[$i] += 1;
     }
  }
}
close (FH);

foreach my $chr (sort keys %chromSizes) {
my %highestCount;	# key is chrName, value is window number of highest count
$highestCount{$chr} = -1;
my $highWater = 0;
$chrPtr = $windows{$chr};
my $winCount = scalar(@$chrPtr);
# printf STDERR "# %s winCount %d\n", $chr, $winCount;
my $lastEnd = $chromSizes{$chr};
for (my $i = 0; $i < $winCount; ++$i) {
  my $count = $chrPtr->[$i];
  if ($count > $highWater) {
    $highWater = $count;
    $highestCount{$chr} = $i;
  }
#  my $end = ($i+1) * $winSize;
#  $end = $lastEnd if ($end > $lastEnd);
#  printf "%s\t%d\t%d\t%d\n", $chr, $i*$winSize, $end, $chrPtr->[$i];
}
my $highWindow = $highestCount{$chr};
if ($highWindow > -1) {
  my $start = $highWindow * $winSize;
  my $end = ($highWindow + 1) * $winSize;
  $end = $lastEnd if ($end > $lastEnd);
  printf "%s\t%d\t%d\t%d\n", $chr, $start, $end, $highWater;
} else {
  printf "%s none\n", $chr;
}
}
