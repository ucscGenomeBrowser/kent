#!/usr/bin/env perl
# n50.pl - calculate N50 values for a two column file


# $Id: n50.pl,v 1.1 2009/08/07 17:50:43 hiram Exp $

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc < 1) {
    printf STDERR "n50.pl - calculate N50 values from a two column file\n";
    printf STDERR "usage:\n\tn50.pl <contigs.sizes> [other.sizes]\n";
    printf STDERR "\twhere <contigs.sizes> is a two column file:\n";
    printf STDERR "\tcontigName size\n";
    exit 255;
}

while (my $sizeFile = shift) {
    my $sizeCount = 0;

    my %sizes;	# key is contigName, value is size

    if ($sizeFile eq "stdin") {
	printf STDERR "#\treading: stdin\n";
	while (my $line = <>) {
	    next if ($line =~ m/^\s*#/);
	    ++$sizeCount;
	    chomp ($line);
	    my ($name, $size) = split('\s+', $line);
	    $sizes{$name} = $size;
	}
    } else {
	printf STDERR "#\treading: $sizeFile\n";
	open (FH, "<$sizeFile") or die "can not read $sizeFile";
	while (my $line = <FH>) {
	    next if ($line =~ m/^\s*#/);
	    ++$sizeCount;
	    chomp ($line);
	    my ($name, $size) = split('\s+', $line);
	    $sizes{$name} = $size;
	}
	close (FH);
    }

    my $totalSize = 0;
    foreach my $key (keys %sizes) {
	$totalSize += $sizes{$key}
    }
    my $n50Size = $totalSize / 2;

    printf STDERR "#\tcontig count: %d, total size: %d, N50 size: %d\n",
	    $sizeCount, $totalSize, $n50Size;

    my $prevContig = "";
    my $prevSize = 0;

    $totalSize = 0;
    my $contigCount =0;
    foreach my $key (sort { $sizes{$b} <=> $sizes{$a} } keys %sizes) {
	++$contigCount;
	$totalSize += $sizes{$key};
	if ($totalSize > $n50Size) {
	    printf "# cumulative\tN50 count\tcontig\tcontig size\n";
	    printf "%d\t%d\t%s\t%d\n",
		$totalSize-$sizes{$key},$contigCount-1,$prevContig, $prevSize;
	    printf "%d one half size\n", $n50Size;
	    printf "%d\t%d\t%s\t%d\n", $totalSize, $contigCount, $key, $sizes{$key};
	    last;
	}
	$prevContig = $key;
	$prevSize = $sizes{$key};
    }
}
