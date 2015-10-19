#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1 ) {
  printf STDERR "ucage: nameToIx.pl <assembly.bed> | sort -u > ix.txt\n";
  printf STDERR "then run ixIxx on ix.txt:\n";
  printf STDERR " ixIxx ix.txt out.ix out.ixx\n";
  exit 255;
}

my $bedFile = shift;

if ($bedFile =~ m/.gz$/) {
  open (FH, "zcat $bedFile|") or die "ERROR: gpToIx.pl can not read '$bedFile'";
} else {
  open (FH, "<$bedFile") or die "ERROR: gpToIx.pl can not read '$bedFile'";
}
while (my $line = <FH>) {
  next if ($line =~ m/^#/);
  chomp ($line);
  my (undef, undef, undef, $name, $rest) = split('\s+', $line, 5);
  if (defined($name)) {
     my $noSuffix=$name;
     $noSuffix =~ s/\.[0-9][0-9]*$//;
     printf "%s\t%s\n", $name, $noSuffix if ($noSuffix ne $name );
  }
}
close (FH);

