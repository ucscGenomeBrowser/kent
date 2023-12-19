#!/usr/bin/perl

# Given 3 tab-separated columns of input (name/acc, date, number of substitutions),
# output sequence name/acc that has suspiciously few mutations for a SARS-CoV-2
# sequence from that date.

use warnings;
use strict;

while (<>) {
  chomp;
  my ($name, $date, $substCount) = split(/\t/);
  next if ($substCount eq "");
  if ($date =~ /^20(19|2\d)(-(\d\d))?(-\d\d)?$/) {
    # Parse date and convert to number of months since epidemic started.
    my ($y, $m) = ($1, $3);
    $m = 1 if (! $m);
    $y -= 20;
    my $epiMonth = 12*$y + $m;
    next if ($epiMonth < 0);
    # Calculate minimum acceptable substitutions (it might be good to consider # of ambigs too)
    my $min = ($epiMonth - 2) * 0.75;
    $min = 0 if ($min < 0);
    if ($substCount < $min) {
      print join("\t", $name, $min, $substCount) . "\n";
    }
  }
}

