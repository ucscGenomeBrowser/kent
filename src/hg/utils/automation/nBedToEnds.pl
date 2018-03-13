#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 2) {
  printf STDERR "usage: nBedToEnds.pl <file.2bit> <endSize> > pairedEnds.tab\n";
  printf STDERR "processes the N gaps in the 2bit file into paired\n";
  printf STDERR "sequences coordinates on each side of the N gaps.\n";
  printf STDERR "Sequences will be <endSize> bases in length when possible.\n";
  exit 255;
}

my $twoBit = shift;
my $endSize = shift;

# verify downStream and upStream coordinates are valid
sub checkOut($$) {
  my ($down, $up) = @_;
  my ($c, $s, $e) = split('[:-]', $down);
  return if ($e - $s < 30);
  ($c, $s, $e) = split('[:-]', $up);
  return if ($e - $s < 30);
  printf "%s\t%s\n", $down, $up;
}

my %chromSize;	# key is chrom name, value is size
my @nBed;	# each element is a paired

open (FH, "twoBitInfo $twoBit stdout | sort|") or die "can not twoBitInfo $twoBit";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $size) = split('\t', $line);
  $chromSize{$chr} = $size;
}
close (FH);

my $downStream = "";
my $upStream = "";
my $prevChrom = "";
my $prevEnd = "";

open (FH, "twoBitInfo -nBed $twoBit stdout | sort -k1,1 -k2,2n |") or die "can not twoBitInfo -nBed $twoBit";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $gapStart, $gapEnd) = split('\t', $line);
  if ($chr ne $prevChrom) {  # starting new chrom, finish off previous fragment
    if (length($downStream)) { # prevEnd to chromEnd is the upstream sequence
       if ($prevEnd < $chromSize{$prevChrom}) { # noop if chrom ends in gap
         my $fragSize = $chromSize{$prevChrom} - $prevEnd;
         $fragSize = $endSize if ($fragSize > $endSize);
         $upStream = sprintf("%s:%d-%d", $prevChrom, $prevEnd ,$prevEnd
                                                               + $fragSize);
         checkOut($downStream, $upStream);
       }
    }
    # and now, this first gap starts a new downstream fragment
    my $fragStart = $gapStart - $endSize;
    $fragStart = 0 if ($fragStart < 0);
    $downStream = sprintf("%s:%d-%d", $chr, $fragStart, $gapStart);
  } else {  # processing same chrom, next gap encountered
    # if we have a downStream defined, this gap makes the upStream fragment
    if (length($downStream)) {
      my $fragEnd = $prevEnd + $endSize;
      $fragEnd = $gapStart if ($fragEnd > $gapStart);
      $upStream = sprintf("%s:%d-%d", $chr, $prevEnd ,$fragEnd);
      checkOut($downStream, $upStream);
    } 
    my $fragStart = $gapStart - $endSize;
    $fragStart = $prevEnd if ($fragStart < $prevEnd);
    $downStream = sprintf("%s:%d-%d", $chr, $fragStart, $gapStart);
  }
  $prevChrom = $chr;
  $prevEnd = $gapEnd;
}
close (FH);

# final segment, last chromosome
if (length($downStream)) { # prevEnd to chromEnd is the upstream sequence
  if ($prevEnd < $chromSize{$prevChrom}) { # noop if chrom ends in gap
      my $fragSize = $chromSize{$prevChrom} - $prevEnd;
      $fragSize = $endSize if ($fragSize > $endSize);
      $upStream = sprintf("%s:%d-%d", $prevChrom, $prevEnd ,$prevEnd
                                                               + $fragSize);
      checkOut($downStream, $upStream);
  }
}
