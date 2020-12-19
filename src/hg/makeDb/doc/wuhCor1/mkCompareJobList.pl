#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: ./allByAllPairs.pl N\n";
  printf STDERR "A_albimanus     68.19   A_aquasalis     68.846969\n";
  printf STDERR "A_albimanus     70.85   A_arabiensis    54.805332\n";
  printf STDERR "A_aquasalis     71.54   A_arabiensis    54.812861\n";

  exit 255;
}

my $N = shift;

my @workList;
my $listCount = 0;

open (FH, "ls -drt kmers/*/*${N}mers.profile.txt.gz | sed -e 's/.gz//;' | sort |") or die "can not read ls -drt kmers/*/*.profile.txt";
while (my $acc = <FH>) {
  chomp $acc;
  $acc =~ s#kmers/##;
  $acc =~ s#/.*##;
  $workList[$listCount++] = $acc;
}
close (FH);
foreach my $db (@workList) {
  printf STDERR "%s\n", $db;
}

my @remainingList = @workList;

for (my $i = 0; $i < $listCount; ++$i) {
  my $s1 = $workList[$i];
  for (my $j = $i+1; $j < $listCount; ++$j) {
    my $s2 = $workList[$j];
    printf "comparePair %s %s %d {check out exists+ compare.%d/%s/%s.txt}\n",
      $s1, $s2, $N, $N, $s1, $s2;
  }
}

printf STDERR "# listCount: %d, job count = %d * (%d / 2) = %d\n", $listCount, $listCount, $listCount, $listCount*($listCount/2);

