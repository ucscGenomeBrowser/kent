#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);
if ($argc != 1) {
  printf STDERR "usage: ./matrixUp.pl file.txt\n";
  printf STDERR "where file.txt is the output of:\n";
  printf STDERR " ./runEm.sh > 1081.txt\n";
  printf STDERR "e.g.: ./matrixUp.pl > 1081.matrix\n";
  exit 255;
}

my %targets;	# key is target name, value is hash that has key
		# query name and value percent target covered

my $file = shift;
open (FH, "<$file") or die "can not read $file";
while (my $line = <FH>) {
  chomp $line;
  my ($target, $targetPercent, $query, $queryPercent) = split('\s+', $line);
  if (! exists($targets{$target})) {
     my %h;
     $h{$target} = 100;
     $targets{$target} = \%h;
  }
  if (! exists($targets{$query})) {
     my %h;
     $h{$query} = 100;
     $targets{$query} = \%h;
  }
  if (exists($targets{$target})) {
     my $hPtr = $targets{$target};
     if (exists($hPtr->{$query})) {
        die "ERROR: 1 multiple value for $query vs. $target";
     }
     $hPtr->{$query} = $targetPercent;
     $hPtr = $targets{$query};
     if (exists($hPtr->{$target})) {
        die "ERROR: 2 multiple value for $target vs. $query";
     }
     $hPtr->{$target} = $queryPercent;
  }
}
close (FH);

open (FH, ">matrixKey.txt")  or die "can not write to matrixKey.txt";
my $index = 0;

foreach my $target (sort keys %targets) {
  printf FH "%d\t%s\n", $index++, $target;
  my $hPtr = $targets{$target};
  my $comma = "";
  foreach my $query (sort keys %$hPtr) {
      printf "%s%s", $comma, $hPtr->{$query};
      $comma = ",";
  }
  printf "\n";
}
close(FH);

__END__
head 2162.txt
GCF_000838505.1_ViralProj14028  9.16    GCF_000839365.1_ViralProj14027  9.66
GCF_000838505.1_ViralProj14028  11.72   GCF_000841625.1_ViralProj14327  11.20
GCF_000838505.1_ViralProj14028  10.81   GCF_000846285.1_ViralProj14406  7.62
GCF_000838505.1_ViralProj14028  8.79    GCF_000861925.1_ViralProj15450  7.62
GCF_000838505.1_ViralProj14028  11.72   GCF_000861945.1_ViralProj15454  8.46

