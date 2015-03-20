#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 3) {
  printf STDERR "usage: ./adjustSizes <targetDb> <queryDb> <N>\n";
  exit 255;
}

my %chrSize1;
my %chrSize2;

my $db1 = shift;
my $db2 = shift;
my $N = shift;

my $overallSize2 = 0;
my $overallSize1 = 0;
my $adjustedSize1 = 0;
my $adjustedSize2 = 0;

open (FH, "</hive/data/genomes/$db1/chrom.sizes") or die "can not read $db1/chrom.sizes";
while (my $line = <FH>) {
  chomp $line;
  my ($name, $size) = split('\s+', $line);
  $chrSize1{$name} = $size;
}
close (FH);

open (FH, "</hive/data/genomes/$db2/chrom.sizes") or die "can not read $db2/chrom.sizes";
while (my $line = <FH>) {
  chomp $line;
  my ($name, $size) = split('\s+', $line);
  $chrSize2{$name} = $size;
}
close (FH);

open (DB1, ">$db1.toFetch.$N.bed") or die "can not write to $db1.$db2/$db1.toFetch.$N.bed";
open (DB2, ">$db2.toFetch.$N.bed") or die "can not write to $db1.$db2/$db2.toFetch.$N.bed";

my $upstreamPad = 5000;

open (FH, "paste $db1.selected.$N.bed $db2.selected.$N.bed|") or die "can not paste two lists";
while (my $line = <FH>) {
  chomp $line;
  my ($name1, $start1, $end1, $name1a, $strand1, $name2, $start2, $end2, $name2a, $strand2) = split('\s+', $line);
  if ($strand1 eq '-') {
     $end1 += $upstreamPad;
     $end1 = $chrSize1{$name1} if ($end1 > $chrSize1{$name1});
  } else {
     $start1 -= $upstreamPad;
     $start1 = 0 if ($start1 < 0);
  }
  if ($strand2 eq '-') {
     $end2 += $upstreamPad;
     $end2 = $chrSize2{$name2} if ($end2 > $chrSize2{$name2});
  } else {
     $start2 -= $upstreamPad;
     $start2 = 0 if ($start2 < 0);
  }
  my $size1 = $end1 - $start1;
  my $size2 = $end2 - $start2;
  $overallSize1 += $size1;
  $overallSize2 += $size2;
  if ($size1 > $size2) {
    my $halfDiff = (1 + $size1 - $size2)/2;
    $start2 -= $halfDiff;
    $end2 += $halfDiff;
    if ($start2 < 0) {
      $end2 -= $start2;
      $start2 = 0;
    }
    if ($end2 > $chrSize2{$name2}) {
        $end2 = $chrSize2{$name2};
    }
    my $newSize2 = $end2 - $start2;
    if ($newSize2 < ($size1 - 1)) {
      my $sizeDiff = $size1 - $newSize2;
      printf STDERR "# warning: %s too small by: %d\n", $name2, $sizeDiff;
    }
  } elsif ($size2 > $size1) {
    my $halfDiff = (1 + $size2 - $size1)/2;
    $start1 -= $halfDiff;
    $end1 += $halfDiff;
    if ($start1 < 0) {
      $end1 -= $start1;
      $start1 = 0;
    }
    if ($end1 > $chrSize1{$name1}) {
        $end1 = $chrSize1{$name1};
    }
    my $newSize1 = $end1 - $start1;
    if ($newSize1 < ($size2 - 1)) {
      my $sizeDiff = $size2 - $newSize1;
      printf STDERR "# warning: %s too small by: %d\n", $name1, $sizeDiff;
    }
  }
  $adjustedSize1 += $end1 - $start1;
  $adjustedSize2 += $end2 - $start2;
#  printf "%s\t%d\t%d\t%s\t%s\t%d\t%d\t%s\n", $name1, $start1, $end1, $name1a, 
#    $name2, $start2, $end2, $name2a;
  printf DB1 "%s\t%d\t%d\t%s\n", $name1, $start1, $end1, $name1a;
  printf DB2 "%s\t%d\t%d\t%s\n", $name2, $start2, $end2, $name2a;
}
close (FH);
close (DB1);
close (DB2);
printf "size1 incoming: %d, adjusted: %d\n", $overallSize1, $adjustedSize1;
printf "size2 incoming: %d, adjusted: %d\n", $overallSize2, $adjustedSize2;
