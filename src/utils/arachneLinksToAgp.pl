#!/usr/bin/env perl
# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/utils/arachneLinksToAgp.pl instead.

# $Id: arachneLinksToAgp.pl,v 1.1 2006/02/13 20:16:33 angie Exp $

# Read in the assembly.links file created by the Arachne assembler 
# and print out AGP.  This assumes that the small sequences are named 
# contig_* and the big sequences are named scaffold_*.  

use warnings;
use strict;

my $seq = 1;
# Note 1-based inclusive coords in AGP:
my $curPos = 1;

while (<>) {
  next if (/^\s*#/);
  if (/^(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(-?\d+)\s+(-?\d+)\s*$/) {
    my ($scafNum, $scafLen, $scafCtgCount, $ctgOrdinalInScaf,
	$ctgNum, $ctgLen, $gapBeforeCtg, $gapAfterCtg) =
	  ($1, $2, $3, $4, $5, $6, $7, $8);
    if ($ctgOrdinalInScaf == 1) {
      $curPos = 1;
    }
    # If there's a gap with positive length before this contig, write gap line:
    if ($gapBeforeCtg > 0) {
      my $endPos = $curPos + $gapBeforeCtg - 1;
      print "scaffold_$scafNum\t$curPos\t$endPos\t$seq\tN\t" .
	    "$gapBeforeCtg\tcontig\tyes\n";
      $curPos += $gapBeforeCtg;
      $seq++;
    }
    # Overlapping contigs are indicated by negative gap lengths:
    if ($gapAfterCtg < 0) {
      $ctgLen += $gapAfterCtg;
    }
    my $endPos = $curPos + $ctgLen - 1;
    print "scaffold_$scafNum\t$curPos\t$endPos\t$seq\tW\t" .
          "contig_$ctgNum\t1\t$ctgLen\t+\n";
    $curPos += $ctgLen;
    $seq++;
  } else {
    die "Parse error (expecting 8 numeric fields, got something else) " .
        "line $.:\n$_\n";
  }
}
