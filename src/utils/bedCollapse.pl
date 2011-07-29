#!/bin/env perl
#
# bedCollapse - combine adjacent bed elements into one element

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/utils/bedCollapse.pl instead.

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc < 1) {
    printf STDERR "usage: bedCollapse.pl <file.bed>\n";
    printf STDERR "will combine adjacent bed elements into one element\n";
    printf STDERR "This is working on only columns 2 and 3, the column 4\n";
    printf STDERR "output is the size of the element.\n";
    exit 255
}

my $file = shift;
my $chr = "";
my $prevEnd = 0;
my $start = 0;
my $end = 0;
my $size = 0;
open (FH, "sort -k1,1 -k2,2n $file|") or die "can not read $file";
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
