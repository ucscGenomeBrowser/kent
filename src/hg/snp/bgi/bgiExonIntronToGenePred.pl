#!/usr/bin/perl -w

# BGI's exon_intron.txt files are almost genePred.tab, but they have an 
# extra column that needs to be clipped, and their start coords need to 
# be translated to 0-based.  

use strict;

while (<>) {
  chop;
  my @words = split();
  $words[3] -= 1;
  $words[5] -= 1 if ($words[5] != 0);
  my @starts = split(',', $words[8]);
  $words[8] = "";
  foreach my $s (@starts) {
    $s -= 1;
    $words[8] .= "$s,";
  }
  $#words = 9;
  print join("\t", @words) . "\n";
}

