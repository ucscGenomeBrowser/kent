#!/bin/env perl
#
# bedInvert - invert a bed file, output regions not covered by the bed file

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/utils/bedInvert.pl instead.

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 2) {
    printf STDERR "usage: bedInvert.pl [chrom.sizes] [file.bed] > invert.bed\n";
    printf STDERR "will output bed elements for gaps between elements in file.bed\n";
    printf STDERR "The file.bed does not have to be sorted, it will be\n";
    printf STDERR "\tsorted here.\n";
    printf STDERR "The chrom.sizes is used to get the last possible element\n";
    printf STDERR "\tto the end of the chromosome.\n";
    exit 255
}

my ($sizesFile, $bedFile) = @ARGV;

my %chromSizes;   # key is chrom name, value is length
if ($sizesFile =~ m/^stdin$/) {
  open (FH, "</dev/stdin") or die "can not read chrom.sizes: stdin";
} elsif ($sizesFile =~ m/.gz$/) {
  open (FH, "zcat $sizesFile|") or die "can not read chrom.sizes: %s", $sizesFile;
} else {
  open (FH, "<$sizesFile") or die "can not read chrom.sizes: %s", $sizesFile;
}
while (my $line = <FH>) {
   chomp $line;
   my ($chr, $size) = split('\s+', $line);
   $chromSizes{$chr} = $size;
}
close (FH);
my %chromDone;   # key is chrom name, value is 1 to indicate done this chrom
my $chr = "";
my $start = 0;
my $end = 0;
my $size = 0;
if ($bedFile =~ m/^stdin$/) {
  open (FH, "grep -v '^ *#' /dev/stdin | sort -k1,1 -k2,2n |") or die "can not read /dev/stdin";
} elsif ($bedFile =~ m/.gz$/) {
  open (FH, "zcat $bedFile | grep -v '^ *#' | sort -k1,1 -k2,2n|") or die "can not read $bedFile";
} else {
  open (FH, "grep -v '^ *#' $bedFile | sort -k1,1 -k2,2n|") or die "can not read $bedFile";
}
while (my $line = <FH>) {
    chomp $line;
    my ($c, $s, $e, $rest) = split('\s+', $line, 4);
    if (length($chr) > 1) {
	if ($chr ne $c) {
            my $chromEnd = $chromSizes{$chr};
            $size = $chromEnd - $end; # may be last element previous chrom
            $chromDone{$chr} = 1;
            printf "%s\t%d\t%d\t%d\n", $chr, $end, $chromEnd, $size if ($size > 0);
	    $chr = $c; $start = $s; $end = $e;
            $size = $start - 0;
            # might be the first gap this new chrom
            $chromDone{$chr} = 1;
            printf "%s\t0\t%d\t%d\n", $chr, $start, $size if ($size > 0);
	} else {
	    if ($s <= $end) {  # next element overlaps or joins previous element
		$end = $e;
	    } else {  # there is a gap here from $end to $s
                $size = $s - $end;
                $chromDone{$chr} = 1;
		printf "%s\t%d\t%d\t%d\n", $chr, $end, $s,
                   $size if ($size > 0);
		$chr = $c; $start = $s; $end = $e;
	    }
	}
    } else {  # first chromosome, first element
	$chr = $c; $start = $s; $end = $e;
        $size = $start - 0;
        $chromDone{$chr} = 1;
        # might be the first gap this new chrom
        printf "%s\t0\t%d\t%d\n", $chr, $start, $size if ($size > 0);
    }
}
# might be final gap on last chrom mentioned
my $chromEnd = $chromSizes{$chr};
$size = $chromEnd - $end;
$chromDone{$chr} = 1;
printf "%s\t%d\t%d\t%d\n", $chr, $end, $chromEnd, $size if ($size > 0);;
close (FH);

# output gaps on chroms never mentioned yet, they had no elements
foreach $chr (keys %chromSizes) {
  next if (exists($chromDone{$chr}));
  printf "%s\t0\t%d\t%d\n", $chr, $chromSizes{$chr}, $chromSizes{$chr};
}
