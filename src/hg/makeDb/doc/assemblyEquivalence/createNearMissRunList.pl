#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

my $argc = scalar(@ARGV);
if ($argc != 3) {
  printf STDERR "usage: ./createNearMissRunList.pl N uniqueCounts.a.b.txt uniqueCounts.b.a.txt\n";
  printf STDERR "where N is the number that can be different\n";
  printf STDERR "e.g.: ./createNearMissRunList.pl 10 \\
  uniqueCounts.ucsc.ensembl.txt uniqueCounts.ensembl.ucsc.txt \\
    > nearMiss.ucsc.ensembl.run.list\n";
  exit 255;
}

my $nDiff = shift;
my $listA = shift;
my $listB = shift;
my $baseA = basename($listA);
my $baseB = basename($listB);
my (undef, $typeA, undef, undef) = split('\.', $baseA);
my (undef, $typeB, undef, undef) = split('\.', $baseB);

# printf STDERR "# createNearMissRunList: $nDiff $listA $listB $typeA $typeB\n";

my %listBUniqueCount;	# key is listB name, value is uniqueline count in idKeys
my %listBFullCount;	# key is listB name, value is total line count in idKeys
my @listBNames;	# array of listB names in order by listBUniqueCount

open (FH, "sort -n $listB|") or die "can not read $listB";
while (my $line = <FH>) {
  chomp $line;
  my ($uniqueCount, $id, $fullCount) = split('\s+', $line);
  $listBUniqueCount{$id} = $uniqueCount;
  $listBFullCount{$id} = $fullCount;
  push @listBNames, $id;
}
close (FH);

# printf STDERR "# number of assemblies in %s %d\n", $listB, scalar(@listBNames);

my %listAUniqueCount;	# key is listA name, value is uniqueline count in idKeys
my %listAFullCount;	# key is listA name, value is total line count in idKeys
my @listANames;	# array of listA names in order by lineCount

open (FH, "sort -n $listA|") or die "can not read $listA";
while (my $line = <FH>) {
  chomp $line;
  my ($uniqueCount, $db, $fullCount) = split('\s+', $line);
  $listAUniqueCount{$db} = $uniqueCount;
  $listAFullCount{$db} = $fullCount;
  push @listANames, $db;
}
close (FH);

foreach my $bName (@listBNames) {
  my $lCount = $listBUniqueCount{$bName};
  my $minCount = $lCount - $nDiff;
  my $maxCount = $lCount + $nDiff;
  $minCount = 1 if ($minCount < 1);
#  printf "%d\t%d-%d\t%s\n", $ensLineCount, $minCount, $maxCount, $bName;
  foreach my $db (sort {$listAUniqueCount{$a} <=> $listAUniqueCount{$b}} keys %listAUniqueCount) {
     if ($listAUniqueCount{$db} <= $maxCount) {
        if ($listAUniqueCount{$db} >= $minCount) {
#           printf STDERR "# %d vs %d\t%s vs %s\n", $ensLineCount, $listAUniqueCount{$db}, $bName, $db;
           printf "./runOne %s %s %s %s\n", $typeB, $bName, $typeA, $db;
        }
     } else {
       last;
     }
  }
}
