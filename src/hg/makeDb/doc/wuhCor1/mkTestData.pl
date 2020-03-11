#!/usr/bin/env perl

use strict;
use warnings;

my %orderKey;	# key is number, value is name
my $keyCount = 0;
my %shortName;	# key is long name, value is short name
my %shortKey;	# key is short name, value is number

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: ./mkTestData.pl N\n";
  printf STDERR "where N is Nmer, will be reading:\n";
  printf STDERR " ../matrixKey.Nmer.txt and ../dataNmer.csv\n";
  exit 255;
}

my $N = shift;

# -rw-rw-r--   1     7270 Dec 23 13:36 matrixKey.31mer.txt
# -rw-rw-r--   1  1049999 Dec 23 13:36 data31mer.csv

# 24	A_stephensi
# 25	Aedes_aegypti
# 26	Aedes_albopictus
# GKP7x01 GWHABKP00000000
# GKW7x01
# 8701    GWHABKP00000000
# 8701    GWHABKW00000000


open (FH, "< ../matrixKey.${N}mer.txt") or die "can not read ../matrixKey.${N}mer.txt";
while (my $line = <FH>) {
  chomp $line;
  my ($key, $name) = split('\s+', $line);
  $orderKey{$key} = $name;
  if ((length($name) > 10) || ($name =~ m/_/) || ($name =~ m/[LM]/)) {
     my @nameParts = split('_', $name);
     my $shortName = $nameParts[1];
     $shortName = $nameParts[0] if (! defined($shortName));
     $shortName =~ s/00000000/70/;
     $shortName =~ s/GWHABKP/8/;
     $shortName =~ s/GWHABKW/9/;
     $shortName =~ s/LC//;
     $shortName =~ s/LR//;
     $shortName =~ s/MN//;
     $shortName =~ s/MT//;
#     $shortName =~ s/0//;
     $shortName =~ s/v//g;
     for (my $i = 1; $i < 4; ++$i) {
        my $tryName = sprintf("%s%d", $shortName, $i);
        if (! defined($shortKey{$tryName})) {
           $shortName = $tryName;
           last;
        }
     }
     printf STDERR "short name is longer than 10 chars ? $shortName\n" if (length($shortName) > 10);
     die "name conflict: $name" if (defined($shortKey{$name}));
     $shortName{$name} = $shortName;
     $shortKey{$keyCount} = $shortName;
  } else {
     printf STDERR "short name is longer than 10 chars ? $name\n" if (length($name) > 10);
     die "name conflict: $name" if (defined($shortKey{$name}));
     $shortName{$name} = $name;
     $shortKey{$keyCount} = $name;
  }
  ++$keyCount;
}
close (FH);

for (my $i = 0; $i < $keyCount; ++$i) {
   printf STDERR "%s\t%s\n",  $shortName{$orderKey{$i}}, $orderKey{$i};
}

my @rows;
my $rowCount = 0;

open (FH, "<../data${N}mer.csv") or die "can not read ../data${N}mer.csv";
while (my $line = <FH>) {
  chomp $line;
  my @a = split(',', $line);
  $rows[$rowCount++] = \@a
}
close (FH);

for (my $i = 0; $i < $keyCount; ++$i) {
   for (my $j = 0; $j < $keyCount; ++$j) {
      next if ($i == $j);
      my $ave = ($rows[$i]->[$j] + $rows[$j]->[$i]) / 2;
      $rows[$i]->[$j] = $ave;
      $rows[$j]->[$i] = $ave;
   }
}

printf "%5d\n", $keyCount;
for (my $i = 0; $i < $keyCount; ++$i) {
   printf "%-10.10s", $shortKey{$i};
   for (my $j = 0; $j < $keyCount; ++$j) {
      my $value = $rows[$i]->[$j];
      printf "%8.4f", (100.0 - $value) / 100.0;
   }
   printf "\n";
}

__END__

printf "%5d\n", $totalCount;
my $countOut = 0;
{
  printf "%-10.10s", $orderKey{$countOut++};
  foreach my $value (@a) {
    printf "%8.4f", (100.0 - $value) / 100.0;
  }
  printf "\n";
}
close (FH);
# 1234567 1012
# Bovine      0.0000  1.6866  1.7198  1.6606  1.5243  1.6043  1.5905

# 0	A_albimanus
# 100,68.19,70.85,68.78,61.97,69.93,81.74,64.83,60.78,70.15,69.18,65.38,58.59,63.98,67.77,60.05,86.46,69.73,71.55,64.93,44.29,59.04,70.54,69.78,66.77,86.68,96.15,62.46,69.16,66.08,67.48,35.08,50.66,70.06,37.06,37.40,41.26,61.13,48.29,28.84,29.78,40.69,74.31,37.44,49.79,45.60,47.17,39.35,46.00,54.09,45.55,41.22,51.73,55.78,45.20,51.98,45.88,64.00,54.63,49.70,63.64,54.12,63.03,62.01,63.85,63.43,62.18,69.28,85.28,50.64,22.01,51.13,45.38,39.31,41.61,50.57,48.96,56.10,58.79,50.84,53.90,52.18,44.80,51.74,87.38,52.68,56.92,49.32,75.34,62.17,57.54,37.07,55.39,15.40,29.68,44.55,60.88,73.87,50.90,47.74,56.23,52.08,52.87,50.85,52.84,48.07,50.21,50.70,48.17,51.23,49.43,49.35,52.54,51.81,53.62,48.39,46.98,55.42,51.49,50.60,51.34,51.21,68.90,40.95
