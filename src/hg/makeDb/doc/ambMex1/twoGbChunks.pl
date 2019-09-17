#!/usr/bin/env perl

use strict;
use warnings;

# 2*1024*1024*1024 = 2147483648

my @sizes;
my @names;

my $totalSequenceSize = 0;

open (FH, "<../../../chrom.sizes") or die "can not read ../../../chrom.sizes";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $size) = split('\s+', $line);
  push @sizes, $size;
  push @names, $chr;
  $totalSequenceSize += $size;
}
close (FH);

my $count = scalar(@sizes);
printf "total incoming contig count: %d, total size: %d\n", $count, $totalSequenceSize;

$totalSequenceSize = 0;
my $totalContigCount = 0;

my $j = $count - 1;
my $i = 0;
my $sizeSum = 0;
my $chunkSize = 0;
my $chunkCount = 0;

my $chunkName = sprintf("chunk%d.list", $chunkCount);
open (CK, ">$chunkName") or die "can not write to $chunkName";

for ( ; $i < $j; ++$i) {
  my $nextSize = $sizeSum + $sizes[$i] + $sizes[$j];
  if ($nextSize < (2 * 1024 * 1024 * 1024)) {
     printf CK "%s\n%s\n", $names[$i], $names[$j];
     $sizeSum = $nextSize;
     $chunkSize += 2;
  } else {
     printf "chunk %d\tcontig count %d\tsize %d\n", $chunkCount, $chunkSize, $sizeSum;
     close (CK);
     $totalSequenceSize += $sizeSum;
     $totalContigCount += $chunkSize;
     ++$chunkCount;

     $chunkName = sprintf("chunk%d.list", $chunkCount);
     open (CK, ">$chunkName") or die "can not write to $chunkName";

     $sizeSum = $sizes[$i] + $sizes[$j];
     $chunkSize = 2;
     printf CK "%s\n%s\n", $names[$i], $names[$j];
  }
  --$j;
}
close(CK);
if ($chunkSize > 0) {
  printf "chunk %d\tcontig count %d\tsize %d\n", $chunkCount, $chunkSize, $sizeSum;
  $totalSequenceSize += $sizeSum;
  $totalContigCount += $chunkSize;
  ++$chunkCount;
}

printf "contigs counted: %d, total sequence: %d at i,j: %d,%d\n", $totalContigCount, $totalSequenceSize, $i, $j;
