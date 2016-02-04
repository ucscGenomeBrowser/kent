#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: gbffToCds.pl GCF_000001405.31_GRCh38.p5_rna.gbff.gz > rna.cds\n";
  exit 255;
}

my $gbff=shift;

my $version = "";
my $cds = "";
open (FH, "zegrep '^VERSION|^  *CDS  *|^  *ncRNA  *|^  *misc_RNA  *|^  *rRNA  *' $gbff|") or die "can not zegrep $gbff";
while (my $line = <FH>) {
  chomp $line;
  if ( $line =~ m/^VERSION/) {
#    if (length($version) && length($cds)) {
#       printf "%s\t%s\n", $version, $cds;
       $cds = "";
       $version = "";
#    }
    (undef, $version, undef) = split('\s+', $line);
  } elsif ( $line =~ m/^  *rRNA/) {
    $line =~ s/^ *//;
    my (undef, $potentialCds, $rest) = split('\s+', $line, 3);
    $cds = "";
    if (defined($rest)) {
      $cds = "";
    } elsif (defined($potentialCds)) {
      $cds = $potentialCds;
    }
    if (length($version) && length($cds)) {
       printf "%s\t%s\n", $version, $cds;
       $cds = "";
       $version = "";
    }
  } elsif ( $line =~ m/^  *misc_RNA/) {
    $line =~ s/^ *//;
    my (undef, $potentialCds, $rest) = split('\s+', $line, 3);
    $cds = "";
    if (defined($rest)) {
      $cds = "";
    } elsif (defined($potentialCds)) {
      $cds = $potentialCds;
    }
    if (length($version) && length($cds)) {
       printf "%s\t%s\n", $version, $cds;
       $cds = "";
       $version = "";
    }
  } elsif ( $line =~ m/^  *ncRNA/) {
    $line =~ s/^ *//;
    my (undef, $potentialCds, $rest) = split('\s+', $line, 3);
    $cds = "";
    if (defined($rest)) {
      $cds = "";
    } elsif (defined($potentialCds)) {
      $cds = $potentialCds;
    }
    if (length($version) && length($cds)) {
       printf "%s\t%s\n", $version, $cds;
       $cds = "";
       $version = "";
    }
  } else {
    $line =~ s/^ *//;
    my (undef, $potentialCds, $rest) = split('\s+', $line, 3);
    $cds = "";
    if (defined($rest)) {
      $cds = "";
    } elsif (defined($potentialCds)) {
      $cds = $potentialCds;
    }
    if (length($version) && length($cds)) {
       printf "%s\t%s\n", $version, $cds;
       $cds = "";
       $version = "";
    }
  }
}
close (FH);

