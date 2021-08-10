#!/usr/bin/env perl

# scan a genePred vs. chrom.sizes and eliminate any oversized records
# typically genes on circular genomes that wrap past end coordinate

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 2) {
  printf STDERR "usage: cleanGenePredOverSized.pl chrom.sizes genePred > clean.genePred\n";
  printf STDERR "\nScan a genePred vs. chrom.sizes and eliminate any oversized records.\n";
  printf STDERR "Typically genes on circular genomes that wrap past end coordinate.\n";
  printf STDERR "Either input file can be .gz or not.\n";
  exit 255;
}

my $chromSizes = shift;
my $genePred = shift;

if ($chromSizes =~ m/.gz$/) {
  open (FH, "zcat $chromSizes|") or die "can not zcat $chromSizes";
} else {
  open (FH, "<$chromSizes") or die "can not read $chromSizes";
}

my %chromSizes;	# key is chr name, value is size
while (my $line = <FH>) {
  chomp $line;
  my ($chrName, $size) = split('\s+', $line);
  $chromSizes{$chrName} = $size;
}
close (FH);

if ($genePred =~ m/.gz$/) {
  open (FH, "zcat $genePred|") or die "can not zcat $genePred";
} else {
  open (FH, "<$genePred") or die "can not read $genePred";
}

while (my $line = <FH>) {
  chomp $line;
  my @a = split('\t', $line);
  my $chrSize = $chromSizes{$a[1]};
  if (($a[3] > $chrSize) || ($a[4] > $chrSize)) {
    printf STDERR "# dropped: %s %s %s %d %d (> %d)\n", $a[0], $a[1], $a[2], $a[3], $a[4], $chrSize;
  } else {
    printf "%s\n", $line;
  }
}
close (FH);

__END__
YP_009695013.1	NC_044775.1	+	162194	162473	162194	162473	1	162194,	162473,	0	rps19	cmpl	cmpl	0,



