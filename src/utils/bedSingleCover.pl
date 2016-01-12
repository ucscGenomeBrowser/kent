#!/usr/bin/env perl
#
# bedSingleCover - combine overlapping bed elements into one element

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/utils/bedSingleCover.pl instead.

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc < 1) {
    printf STDERR "usage: bedSingleCover.pl [file.bed] > singleCover.bed\n";
    printf STDERR "will combine overlapping bed elements into one element\n";
    printf STDERR "No need to pre-sort the bed file, it will be sorted here\n";
    printf STDERR "result is four columns: chr start end size\n";
    printf STDERR "where size is the size of the element: size=end-start\n";
    printf STDERR "\nTo obtain a quick featureBits like measurement of\n";
    printf STDERR "this singleCover result, using awk:\n";
    printf STDERR " bedSingleCover.pl file.bed |";
    printf STDERR " awk '{sum+=\$3-\$2}END{printf \"%%d bases\\n\", sum}'\n";
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
	    if ($s <= $end) {
		$end = $e if ($e > $end);
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
