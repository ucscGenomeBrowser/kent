#!/bin/env perl
#
# bedCollapse - combine adjacent bed elements into one element

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/utils/bedCollapse.pl instead.

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc < 1) {
    printf STDERR "usage: bedCollapse.pl [file.bed] > collapsed.bed\n";
    printf STDERR "will combine adjacent bed elements into one element\n";
    printf STDERR "This is working on only columns 2 and 3, the column 4\n";
    printf STDERR "output is the size of the element.\n";
    printf STDERR "NOTE: This is not like bedSingleCover.pl, this collapse\n";
    printf STDERR "      only works on exactly adjacent elements as determined\n";
    printf STDERR "      by column 2 from 'sort -k1,1 -k2,2n' of the input bed file\n";
    printf STDERR "      where chromEnd of the previous element is == chromStart\n";
    printf STDERR "      of the next element.\n";
    exit 255
}

my $bedFile = shift;
my $chr = "";
my $prevEnd = 0;
my $start = 0;
my $end = 0;
my $size = 0;
if ($bedFile =~ m/^stdin$/) {
  open (FH, "grep -v '^ *#' /dev/stdin | sort -k1,1 -k2,2n |") or die "can not read /dev/stdin";
} else {
  open (FH, "grep -v '^ *#' $bedFile | sort -k1,1 -k2,2n|") or die "can not read $bedFile";
}
while (my $line = <FH>) {
    chomp $line;
    my ($c, $s, $e, $rest) = split('\s+', $line, 4);
    $size = $end - $start;
    if (length($chr) > 1) {
	if ($chr ne $c) {
	    printf "%s\t%d\t%d\t%d\n", $chr, $start, $end, $size;
	    $chr = $c; $start = $s; $end = $e;
	} else {
	    if ($s == $end) {
		$end = $e;
	    } else {
		printf "%s\t%d\t%d\t%d\n", $chr, $start, $end, $size;
		$chr = $c; $start = $s; $end = $e;
	    }
	}
    } else {
	$chr = $c; $start = $s; $end = $e;
    }
}
$size = $end - $start;
printf "%s\t%d\t%d\t%d\n", $chr, $start, $end, $size;
close (FH);
