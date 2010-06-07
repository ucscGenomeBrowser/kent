#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/utils/mergeOverlapBed4.pl instead.

# $Id: mergeOverlapBed4.pl,v 1.1 2007/10/10 17:21:13 angie Exp $

use warnings;
use strict;

if (scalar(@ARGV) < 1 || grep {/^-h/} @ARGV) {
  die "
usage: mergeOverlapBed4.pl inputFile
Expects BED input with at least 4 fields.  For each {chr,name} pair,
merges overlapping ranges and prints out sorted BED4 to stdout.
inputFile can be - or stdin to read from stdin.

";
}
grep {s/^stdin$/-/i} @ARGV;

my %item2coords;
while (<>) {
  chomp;
  my ($chrom, $start, $end, $name) = split;
  die "Sorry, input must have at least 4 fields of BED.\n" if (! $name);
  push @{$item2coords{"$chrom.$name"}}, [$start, $end];
}

my @results;
foreach my $item (keys %item2coords) {
  my @sortedCoords = sort { $a->[0] <=> $b->[0] } @{$item2coords{$item}};
  my ($chrom, $name) = split(/\./, $item);
  my ($mergeStart, $mergeEnd) = @{shift @sortedCoords};
  foreach my $rangeRef (@sortedCoords) {
    my ($rangeStart, $rangeEnd) = @{$rangeRef};
    next if ($rangeEnd <= $mergeEnd);
    if ($rangeStart > $mergeEnd) {
      push @results, [$chrom, $mergeStart, $mergeEnd, $name];
      ($mergeStart, $mergeEnd) = ($rangeStart, $rangeEnd);
    } else {
      $mergeEnd = $rangeEnd;
    }
  }
  push @results, [$chrom, $mergeStart, $mergeEnd, $name] if ($mergeEnd);
}

sub bed4Cmp {
  # For sorting by chrom, chromStart, and names -- reverse order for names
  # because that results in the correct order in hgTracks display.
  return $a->[0] cmp $b->[0] ||
    $a->[1] <=> $b->[1] ||
      $b->[3] cmp $a->[3];
}

foreach my $r (sort bed4Cmp @results) {
  my ($chrom, $start, $end, $name) = @{$r};
  print "$chrom\t$start\t$end\t$name\n";
}

