#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
    printf STDERR "usage: ./splitRegions.pl mafSplit.bed\n";
    exit 255;
}

my $file = shift;

# printf "browser hide all\n";
# printf "browser position chr1:1-34238132\n";
printf "track name=20wayRegions visibility=pack " .
	"description='hg38 20-way multiple alignment split regions'\n";

my %chromSizes;
open (FH,"<../../../chrom.sizes") or die "can not read chrom.sizes";
while (my $line = <FH>) {
    chomp $line;
    my ($name, $size) = split('\s+',$line);
    $chromSizes{$name} = $size;
}
close (FH);

if (1 == 0) {
foreach my $key (keys %chromSizes) {
    printf "%s\t%d\n", $key, $chromSizes{$key};
}
}

my $chrName = "";
my $chrStart = 0;
my $chrEnd = 0;
my $chrPart = 0;
my %chrDone;

open (FH, "sort -k1,1 -k2,2n $file|") or die "can not read $file";
while (my $line = <FH>) {
    chomp $line;
    my ($chr, $start, $end) = split('\s+', $line);
    if (0 == length($chrName)) {
	printf "%s\t%d\t%d\t%s.%02d\n", $chr, $chrStart, $start, $chr, $chrPart;
	$chrName = $chr;
	$chrStart = $start;
	$chrPart += 1;
	$chrDone{$chrName} = 1;
    } elsif ($chrName ne $chr) {
	printf "%s\t%d\t%d\t%s.%02d\n", $chrName, $chrStart, $chromSizes{$chrName}, $chrName, $chrPart;
	$chrName = $chr;
	$chrStart = 0;
	$chrPart = 0;
	$chrDone{$chrName} = 1;
	printf "%s\t%d\t%d\t%s.%02d\n", $chr, $chrStart, $start, $chr, $chrPart;
	$chrStart = $start;
	$chrPart += 1;
    } else {
	printf "%s\t%d\t%d\t%s.%02d\n", $chrName, $chrStart, $start, $chrName, $chrPart;
	$chrStart = $start;
	$chrPart += 1;
    }
}
close (FH);
printf "%s\t%d\t%d\t%s.%02d\n", $chrName, $chrStart, $chromSizes{$chrName}, $chrName, $chrPart;

foreach my $key (keys %chromSizes) {
    if (!exists($chrDone{$key})) {
	printf "%s\t0\t%d\t%s.00\n", $key, $chromSizes{$key}, $key;
    }
}
