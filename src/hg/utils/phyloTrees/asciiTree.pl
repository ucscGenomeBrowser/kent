#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

sub usage() {
    printf STDERR "usage: asciiTree.pl <file.nh>\n";
    printf STDERR "will print out the tree with one species per line\n";
    printf STDERR "conveniently indented to imitate a graphical image.\n";
    exit 255;
}

if ($argc != 1) {
   usage;
}

my $file = shift;

my $nhString = "";

open (FH, "<$file") or die "can not read $file";
while (my $line = <FH>) {
  $nhString .= $line;
}
close (FH);

$nhString =~ s/\s//g;
my @species = split(',', $nhString);
my $indent = 0;
for (my $i = 0; $i < scalar(@species)-1; ++$i) {
  for (my $j = 0; $j < $indent; ++$j) {
    print " ";
  }
  my $thisLine = $species[$i];
  my $j = 0;
  if (substr($thisLine,$j++,1) ne "(") {
    $indent -= 1;
  } else {
    while (substr($thisLine,$j++,1) eq "(") {
      ++$indent;
    }
  }
  printf "%s,\n", $species[$i];
}
printf "%s\n", $species[-1];
