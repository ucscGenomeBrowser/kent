#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/utils/cloneEndParse.pl instead.

# $Id: cloneEndParse.pl,v 1.1 2006/05/05 22:58:25 hiram Exp $

# File: convertTxt
# Author: Heather Trumbower
# Based on /cluster/bin/scripts/convertBacEndPairInfo
# Date: May 2005
# Description: Converts *.txt files to 
# cloneEndsPair.txt and cloneEndsSingle.txt file used for creating 
# BAC End Pairs tracks
#	turned on warnings and strict, and annotated 2006-05-05 - Hiram

use warnings;
use strict;

# Usage message
if ($#ARGV < 0) {
  print stderr "USAGE: convertTxt <*.txt>\n";
  exit(1);
}

$file = shift(@ARGV);
open(FILE, "$file") || die("Could not open $file\n");


# Read in and record end info
print stderr "Reading in end info\n";
open (OUT, ">changes.txt");
while ($line = <FILE>) {
  next if (substr($line,0,1) eq "#");
  chomp($line);
  ($clone, $acc, $gi, $lib, $length, $length2, $vector, $end, $date) = split('\t',$line);
  ($acc, $ver) = split(/\./,$acc);
  $end =~ tr/a-z/A-Z/;

  $found{$clone} = 1;
  $clone{$acc} = $clone;
  $printa{$acc} = 0;
  $print{$clone} = 0;
  $end{$acc} = $end;
  if (&isForward($end)) {
    $t7{$clone} .= "$acc,";
  } elsif (&isReverse($end)) {
    $sp6{$clone} .= "$acc,";
  } elsif ($end) {
    print stderr "End $end for $acc / $clone\n";
  }
}
close(OUT);

# Print out pairs
open(OUT, ">cloneEndPairs.txt");
print stderr "Writing out pair info\n";
foreach $clone (keys %found) {
  if ($t7{$clone} && $sp6{$clone}) {
    print OUT "$t7{$clone}\t$sp6{$clone}\t$clone\n";
    $print{$clone} = 1;
    @acc = split(/\,/,$t7{$clone});
    for ($i = 0; $i <= $#acc; $i++) {
       $printa{$acc[$i]} = 1;
    }
    @acc = split(/\,/,$sp6{$clone});
    for ($i = 0; $i <= $#acc; $i++) {
       $printa{$acc[$i]} = 1;
    }
    $pair++;
  }
}
close(OUT);

# Print out singletons
print stderr "Writing out singleton info\n";
open(OUT, ">cloneEndSingles.txt");
foreach $acc (keys %printa) {
  $clone = $clone{$acc};
  if (!$printa{$acc}) {
    if (&isForward($end{$acc})) {
      print OUT "$acc\t$clone\tT7\n";
    } elsif (&isReverse($end{$acc})) {
      print OUT "$acc\t$clone\tSP6\n";
    } else {
      print OUT "$acc\t$clone\tUNK\n";
    }
    $single++;
  }
}
close(OUT);
print stderr "$pair pairs and $single singles\n";


sub isForward {
    $end = shift(@_);
    if (($end =~ /^T7/) || ($end eq "F")) {
	return 1;
    } else {
	return 0;
    }
}


sub isReverse {
    if (($end =~ /^SP6/) || ($end eq "R") || ($end =~ /^TJ/)) {
	return 1;
    } else {
	return 0;
    }
}


