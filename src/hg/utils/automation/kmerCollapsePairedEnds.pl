#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 2) {
   printf STDERR "usage: collapsePairedEnds.pl minSize path/to/file.bed.gz | gzip -c > tmp/file.bed.gz\n";
   printf STDERR "takes input file that are paired kmer ends, will collapse\n";
   printf STDERR "those ends down when they overlap each other and have\n";
   printf STDERR "the same gap between the ends.  They can collapse completely\n";
   printf STDERR "into a repeat sequence, such sequences are discarded.\n";
   printf STDERR "print out only those pairs where the duplicated sequence\n";
   printf STDERR "is greater than minSize\n";
   exit 255;
}

my $minSize = shift;
my $bedFile = shift;
my $disappear = 0;

my $prevGapSize = 0;
my $prevDupSize = 0;
my $prevStart = 0;
my $chrName = "";
my $itemStart = 0;
my $itemDupSize = 0;
my $itemGapSize = 0;

sub itemOut($$$$) {
  my ($name, $start, $dupSize, $gapSize) = @_;
  if ($disappear) {
#     printf STDERR "# disappeared into repeat sequence\n";
#     printf STDERR "%s\t%d\t%d\n", $name, $start, $dupSize;
     $disappear = 0;
  } else {
     if ( ($dupSize > $minSize) && ($gapSize > 0) ) {
       printf "%s\t%d\t%d\t%s:%d-%d\t%d\t+\t%d\t%d\t0\t2\t%d,%d\t0,%d\n",
          $name, $start, $start + 2 * $dupSize + $gapSize,
          $name, 1 + $start, $start + 2 * $dupSize + $gapSize,
          $dupSize,
          $start, $start + 2 * $dupSize + $gapSize,
          $dupSize, $dupSize, $dupSize+$gapSize;
     }
  }
}

open (FH, "zcat $bedFile | sort -k2,2n|") or die "can not zcat $bedFile";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $start, $end, $name, $dupSize, $rest) = split('\s+', $line, 6);
  my $gapSize = $end - $start - 2*$dupSize;
  if ($prevGapSize > 0) {
     if ($gapSize == $prevGapSize) {
       if ($start < ($itemStart + $itemDupSize)) {
          $itemDupSize = $dupSize + ($start - $itemStart);
          $itemGapSize = $gapSize - ($start - $itemStart);

          $disappear = 1 if ($itemGapSize < 1);
       } else {
          itemOut($chrName, $itemStart, $itemDupSize, $itemGapSize);
          $itemStart = $start;
          $itemDupSize = $dupSize;
          $itemGapSize = $gapSize;
          $disappear = 0;
       }
     } else {
       itemOut($chrName, $itemStart, $itemDupSize, $itemGapSize);
       $itemStart = $start;
       $itemDupSize = $dupSize;
       $itemGapSize = $gapSize;
       $disappear = 0;
     }
  } else {
    $itemStart = $start;
    $itemDupSize = $dupSize;
    $itemGapSize = $gapSize;
    $disappear = 0;
  }
  $prevGapSize = $gapSize;
  $prevDupSize = $dupSize;
  $prevStart = $start;
  $chrName = $chr;
}
close (FH);

# output last item

itemOut($chrName, $itemStart, $itemDupSize, $itemGapSize);
