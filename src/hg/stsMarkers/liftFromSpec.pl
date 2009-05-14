#!/usr/bin/env perl
# liftFromSpec.pl  - take 2bit part specifications from partitionSequence.pl
#	and create a lift file
#
#	These 2bit part specifications look like:
#	chr10:0-40020000
#	chr10:120000000-135534747
#	chr11_gl000202_random
#	chr17_ctg5_hap1
#	chrM

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/stsMarkers/liftFromSpec.pl instead.

# $Id: liftFromSpec.pl,v 1.1 2009/05/05 22:02:41 hiram Exp $

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 2) {
    print STDERR "usage: ./liftFromSpec.pl db specFile.txt > result.lift\n";
    print STDERR "where specFile.txt lines are like:\n";
    print STDERR "\tchrN:start-end\n";
    print STDERR "\tchrJ\n";
    print STDERR "db is a database name to find /hive/data/genomes/db/chrom.sizes file\n";
    print STDERR "db is a database name to find /hive/data/genomes/db/chrom.sizes file\n";
    exit 255;
}

my $db = shift;
my $specFile = shift;
my $chromSizes = "/hive/data/genomes/$db/chrom.sizes";
my %chromSizes;

print STDERR "#\treading chrom.sizes: $chromSizes\n";
open (FH,"<$chromSizes") or die "can not read chrom.sizes file: '$chromSizes'";
while (my $line = <FH>) {
    my ($chr, $size) = split('\s+', $line);
    $chromSizes{$chr} = $size;
}
close (FH);

print STDERR "#\treading specFile: $specFile\n";
open (FH,"<$specFile") or die "can not read '$specFile'";
while (my $line = <FH>) {
    chomp $line;
    my $chr = "";
    my $pos = "";
    my $start = 0;
    ($chr, $pos) = split(':', $line);
    $chr = $line if (length($chr) < 1);
    my $size = $chromSizes{$chr};
    my $end = $size;
    ($start, $end) = split('-', $pos) if ($pos);
    my $range = $end - $start;
    printf "%d\t%s\t%d\t%s\t%d\n", $start, $line, $range, $chr, $size;
}
close (FH);
