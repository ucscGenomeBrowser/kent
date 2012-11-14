#!/usr/bin/env perl

## run in the directory bed/cytoBand/ where cytoBand.bed has been
##	produced, verifies coordinates are OK relative to chrom.sizes

#	$Id: cytoBandVerify.pl,v 1.1 2007/08/15 18:50:11 hiram Exp $

use warnings;
use strict;

open (FH, "<../../chrom.sizes") or die "can not open ../../chrom.sizes";

my %chromInfo;	# key is chrom name, value is size
while (my $line = <FH>) {
    chomp $line;
    my ($chr, $size) = split('\s+',$line);
    $chromInfo{$chr} = $size;
}
close (FH);

# foreach my $chr (sort (keys %chromInfo)) {
#   print "$chr\t$chromInfo{$chr}\n"; 
#}

my %zeros;  # key is chrom, value unimportant, indicates start==0 found
my %ends;   # key is chrom, value unimportant, indicates end==chromSize found

open (FH, "<cytoBand.bed") or die "can not open cytoBand.bed";

while (my $line = <FH>) {
    chomp $line;
    my ($chr, $start, $end, $name, $band) = split('\s+', $line);
    die "start < 0 at $line" if ($start < 0);
    die "end > $chromInfo{$chr} at $line" if ($end > $chromInfo{$chr});
    $zeros{$chr} = 1 if (0 == $start);
    $ends{$chr} = 1 if ($chromInfo{$chr} == $end);
}

my $chrCount = 0;
#  all zeros and ends covered ?
foreach my $chr (sort (keys %chromInfo)) {
    next if ($chr =~ m/random/);
    next if ($chr =~ m/chrM/);
    next if ($chr =~ m/chrUn/);
    die "no zero coordinate on chrom $chr" if (!exists($zeros{$chr}));
    die "no end coordinate on chrom $chr" if (!exists($ends{$chr}));
    ++$chrCount;
}

print "everything checks out OK on $chrCount chroms\n";
