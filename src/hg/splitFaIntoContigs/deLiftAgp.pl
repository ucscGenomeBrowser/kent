#!/usr/local/bin/perl -w

use strict;

my $lftFname = shift @ARGV;
my $agpFname = shift @ARGV;

if ((! defined $lftFname) || (! defined $agpFname)) {
  die "Error:  must have 2 arguments, lift file and agp file.\n";
}

open(LFT, "<$lftFname") ||
  die "Couldn't open lift file \"$lftFname\"\n";

open(AGP, "<$agpFname") ||
  die "Couldn't open agp file \"$agpFname\"\n";

my @lifts = ();
while (<LFT>) {
  my ($chromStart, $ctgName, $ctgSize, $chromName, $chromSize) = split(" ");
  # hack (what's a better way?): strip chr/ out of $ctgName
  $ctgName =~ s@.*/@@;
  push @lifts, [$chromStart, $ctgName, $ctgSize, $chromName, $chromSize];
}
close(LFT);

while(<AGP>) {
  my @words = split(" ");
  ($words[0], $words[1], $words[2]) = &deLift($words[0], $words[1], $words[2]);
  print join("\t", @words) . "\n";
}
close(AGP);


sub deLift {
  my ($chromName, $chromStart, $chromEnd) = @_;
  foreach my $liftRef (@lifts) {
    my ($lChrStart, $lCtgName, $lCtgSize, $lChrName, $lChrSize) = @{$liftRef};
    if (($lChrName eq $chromName) && ($chromStart > $lChrStart) &&
	($chromStart <= ($lChrStart + $lCtgSize))) {
      $chromName   = $lCtgName;
      $chromStart -= $lChrStart;
      $chromEnd   -= $lChrStart;
      last;
    }
  }
  return($chromName, $chromStart, $chromEnd);
}

