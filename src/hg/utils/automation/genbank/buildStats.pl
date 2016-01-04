#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;

# from Perl Cookbook Recipe 2.17, print out large numbers with comma
# delimiters:
sub commify($) {
    my $text = reverse $_[0];
    $text =~ s/(\d\d\d)(?=\d)(?!\d*\.)/$1,/g;
    return scalar reverse $text
}

my $argc = scalar(@ARGV);

if ($argc < 1) {
    printf STDERR "buildStats.pl - calculate build stats from chrom.sizes\n";
    printf STDERR "usage:\n\tbuildStats.pl <chrom.sizes>\n";
    printf STDERR "\twhere <chome.sizes> is a two column file:\n";
    printf STDERR "\tchrName size\n";
    exit 255;
}

my $ix = 0;
while (my $sizeFile = shift) {
    my $contigCount = 0;

    my %sizes;	# key is contigName, value is size

    if ($sizeFile eq "stdin") {
	while (my $line = <>) {
	    next if ($line =~ m/^\s*#/);
	    ++$contigCount;
	    chomp ($line);
	    my ($name, $size, $rest) = split('\s+', $line, 3);
	    my $key = sprintf("%s_X_%d", $name, $ix++);
	    $sizes{$key} = $size;
	}
    } else {
	open (FH, "<$sizeFile") or die "can not read $sizeFile";
	while (my $line = <FH>) {
	    next if ($line =~ m/^\s*#/);
	    ++$contigCount;
	    chomp ($line);
	    my ($name, $size, $rest) = split('\s+', $line, 3);
	    my $key = sprintf("%s_X_%d", $name, $ix++);
	    $sizes{$key} = $size;
	}
	close (FH);
    }

    my $totalSize = 0;
    foreach my $key (keys %sizes) {
	$totalSize += $sizes{$key}
    }
    my $n50Size = $totalSize / 2;

    my $genomeSize = $totalSize;

    my $prevContig = "";
    my $prevSize = 0;

    $totalSize = 0;
    # work through the sizes until reaching the N50 size
    foreach my $key (sort { $sizes{$b} <=> $sizes{$a} } keys %sizes) {
	$totalSize += $sizes{$key};
	if ($totalSize > $n50Size) {
	    my $prevName = $prevContig;
	    $prevName =~ s/_X_[0-9]+//;
	    my $origName = $key;
	    $origName =~ s/_X_[0-9]+//;
            # the third number here is N50 size
            printf STDERR "%d\t%d\t%d\n", $contigCount, $genomeSize, $sizes{$key};
	    last;
	}
	$prevContig = $key;
	$prevSize = $sizes{$key};
    }
}
