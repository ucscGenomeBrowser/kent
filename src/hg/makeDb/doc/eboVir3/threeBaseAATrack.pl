#!/usr/bin/env perl
#
#  scan muPIT.bed single base locations and turn into three base codon
#                 encoded AAs for more useful track clicks on codons
#

use strict;
use warnings;
my %codon;

sub twoBitSequence($$$) {
  my ($chr, $start, $end) = @_;
  my $seq = "";
  open (SQ, "twoBitToFa -seq=$chr -start=$start -end=$end /hive/data/genomes/eboVir3/eboVir3.2bit stdout|grep -v '^>'|") or die "can not run twoBitToFa $chr, $start, $end";
  while (my $line = <SQ>) {
     next if ($line =~ m/^>/);
     chomp $line;
     $seq .= $line;
  }
  close (SQ);
  my $SEQ = uc($seq);
  my $AA = "";
  if (exists($codon{$SEQ})) {
    $AA = $codon{$SEQ};
  } else {
    die "can not find codon for $SEQ";
  }
  return ($SEQ, $AA);
}
open (FH, "<codon.table.txt") or die "can not read codon.table.txt";
while (my $line = <FH>) {
  chomp $line;
  my ($dna, $AA) = split('\s+', $line);
  $codon{uc($dna)} = $AA;
}
close (FH);

open (FH, "sort -u muPIT.bed | sort -k2,2n|") or die "can not read muPIT.bed";
my $itemStart = -1;
my $itemEnd = -1;
my $lastDone = 1;
my $chrName = "";
while (my $line = <FH>) {
  chomp $line;
  my ($chr, $start, $end, $name) = split('\s+', $line);
  if ($itemStart > -1) {
    my $size = $end - $itemStart;
    if ($size > 3) {  # exceeded count of three, output last item
      my ($sequence, $AA) = twoBitSequence($chr, $itemStart, $itemEnd);
      printf "%s\t%d\t%d\t%s/%s\n", $chr, $itemStart, $itemEnd, $sequence, $AA;
      $itemStart = $start;
      $itemEnd = $end;
      $lastDone = 1;
    } else {
      $itemEnd = $end;
      $lastDone = 0;
    }
  } else {
    $itemStart = $start;
    $itemEnd = $end;
    $lastDone = 0;
    $chrName = $chr;
  }
}
close (FH);
if (! $lastDone) {
  my ($sequence, $AA) = twoBitSequence($chrName, $itemStart, $itemEnd);
  printf "%s\t%d\t%d\t%s/%s\n", $chrName, $itemStart, $itemEnd, $sequence, $AA;
}
