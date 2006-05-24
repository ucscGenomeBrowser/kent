#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file -- 
# edit ~/kent/src/hg/utils/createNcbiCytoBand.pl instead.

# File: createNcbiCytoBand
# Author: Terry Furey
# Date: 11/14/2003
# Description: Convert NCBI files into cytoBand track
#	added to source tree 2006-04-27 - Hiram

use strict;
use warnings;

# USAGE message
if (scalar(@ARGV) != 1) {
    print STDERR "createNcbiCytoBand.pl - Convert NCBI ideogram file into cytoBand track\n";
    print STDERR "usage: createNcbiCytoBand <ncbi ideogram file>\n";
    print STDERR "   creates cytoBand.bed file\n";
    exit(255);
}
my $file = shift(@ARGV);
open(FILE, "<$file") or die "Could not open $file: $!";

# Read in file and output cytoBand.bed file
open(OUT, ">cytoBand.bed") or die "Could not open >cytoBand.bed: $!";
my $prevstart;
my $prevend;
while (my $line = <FILE>) {
    next if ($line =~ /^#/);
    chomp($line);
    my ($chr, $arm, $band, $iscntop, $iscnbot, $start, $end, $stain, $density,
	@rest) = split("\t",$line); 
    next if ($band =~ m/ter/);
    # Make data 0-based
    print STDERR "Start=$start is <1, $file line $." if ($start < 1);
    $start-- if ($start == 1);
    # Checks for observed errors in NCBI file
    if (defined $prevstart && $start == $prevstart) {
	$start = $prevend + 1;
	warn "Tweaking start from $prevstart to $start, line $.\n";
    }
    if (!$stain) {
	$stain = "gneg";
    }
    if (!$density) {
	$density = "";
    }
    # Only print lines with band data
    if ($chr =~ /[0-9,X,Y]/) {
	$band =~ s/\://;
	print OUT "chr$chr\t$start\t$end\t$arm$band\t$stain$density\n";
    } else {
	warn "Skipping line $. because $chr does not look like a real chrom.\n";
    }
    $prevstart = $start;
    $prevend = $end;
}
