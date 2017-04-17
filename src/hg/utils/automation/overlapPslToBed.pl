#!/usr/bin/env perl

# specialized pslToBed conversion for the gapOverlap track
# see notes in usage message below for details

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
  printf STDERR "usage: overlapPslToBed.pl file.psl > file.bed\n\n";
  printf STDERR "specialized PSL to BED conversion for the gapOverlap track.\n";
  printf STDERR "The target and query results in the PSL result are used to\n";
  printf STDERR "label the bed item in the output and to construct a two\n";
  printf STDERR "element linked feature bed item.  These PSL results are\n";
  printf STDERR "single perfect matches as filtered by previous processes.\n";
  exit 255;
}

my $pslFile = shift;
$pslFile = "/dev/stdin" if ($pslFile eq "stdin");  # kent command behavior

open (FH, "<$pslFile") or die "can not read $pslFile";
while (my $line = <FH>) {
  chomp $line;
  my ($matches, $misMatches, $repMatches, $nCount, $qNumInsert, $qBaseInsert,
      $tNumInsert, $tBaseInsert, $strand, $qName, $qSize, $qStart, $qEnd,
      $tName, $tSize, $tStart, $tEnd, $blockCount, $blockSizes,
      $qStarts, $tStarts) = split('\s', $line);
  if ($blockCount > 1) {
     printf STDERR "# ERROR: blockCount > 1 == %d\n", $blockCount;
     printf STDERR "# %s\n", $line;
     die ("PSL file has not been filtered correctly.");
  }
  my ($qC, $qS, $qE) = split('[:-]', $qName);
  my ($tC, $tS, $tE) = split('[:-]', $tName);
  printf "%s\t%d\t%d\t%s:%d-%d\t%d\t+\t%d\t%d\t0\t2\t%d,%d\t0,%d\n", $tC, $tE-$matches, $qS+$matches, $tC, $tE-$matches+1, $qS+$matches, $matches, $tE-$matches, $qS+$matches, $matches, $matches, $qS-$tE+$matches;
}
close (FH);
