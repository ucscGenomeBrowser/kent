#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/fixNcbiFuncCoords.pl instead.

use warnings;
use strict;

sub usage($) {
  warn "
usage: zcat ncbiFuncAnnotations.txt.gz | fixNcbiFuncCoords.pl insertionsFile | sort ...

insertionsFile is a BED4 file produced by doDbSnp.pl
(ncbiFuncInsertions.ctg.bed.gz), usually gzipped (.gz).
stdin should come from ncbiFuncAnnotations.txt(.gz), another doDbSnp.pl product.
Output typically is sent to a sort command, see e.g. canFam3.txt

";
  exit $_[0];
}

my %coding = (2=>1, 3=>1, 4=>1, 8=>1, 9=>1, 41=>1, 42=>1, 43=>1, 44=>1, 45=>1);

# Exactly one argument required:
my $insertionsFile = shift @ARGV;
usage (1) if (! defined $insertionsFile || defined shift @ARGV);

# First, read insertionsFile into a hash for testing whether a SNP mapping is an insertion:
my %insertions;
my $insertionsF = ($insertionsFile =~ /\.gz$/) ? "zcat $insertionsFile |" : "<$insertionsFile";
open(my $INS, $insertionsF) || die "fixNcbiFuncCoords.pl: can't open '$insertionsF': $!";
while (<$INS>) {
  chomp;
  $insertions{$_} = 1;
}
close($INS);

# Next, process stdin; adjust NCBI's coord convention of 0-based,
# fully-closed with 2-base-wide insertion coords to UCSC's 0-based,
# half-open with 0-base-wide insertion coords.
# For anything except an insertion, we need to add 1 to the end coord.
# For an insertion, we need to add 1 to the start coord.  Make a hash
# of the insertion IDs, then look up each ID in
# ncbiFuncAnnotations.txt to tell which transform to apply.
while (<>) {
  chomp;
  my @w = split("\t");	# id, ctg, start, end, ...
  next unless $coding{$w[5]};
  my $bed4 = join("\t", $w[1], $w[2], $w[3], $w[0]);
  if (exists $insertions{$bed4} && $w[3] == $w[2]+1) {
    # 2-base insertions: increment start coord to get start==end
    $w[2]++;
  } else {
    # increment end coord to get half-open
    $w[3]++;
  }
  print join("\t", @w) . "\n";
}
