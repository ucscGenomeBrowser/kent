#!/usr/bin/env perl

# Read a series of GenBank flat file LOCUS records and write each one to a file.

use warnings;
use strict;

my $outF;

while (<>) {
  if (/^LOCUS\s+(\w+)/) {
    my $acc = $1;
    close $outF if ($outF);
    open($outF, ">$acc.gbff") || die;
    print $outF $_;
  } elsif ($outF) {
    print $outF $_;
  } else {
    die "Expected input to begin with a LOCUS line but got this:\n$_";
  }
}
close $outF if ($outF);
