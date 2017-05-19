#!/usr/bin/env perl
# File: createNcbiCytoBand
# Author: Terry Furey
# Date: 11/14/2003
# Description: Convert NCBI files into cytoBand track

# USAGE message
if ($#ARGV != 0) {
    print stderr "USAGE: createNcbiCytoBand <ncbi file>\n";
    exit(1);
}
$file = shift(@ARGV);
open(FILE, "$file") || die("Could not open $file\n");

# Read in file and output cytoBand.bed file
open(OUT, ">cytoBand.bed");
$prevstart = "";
$prevend = "";
while ($line = <FILE>) {
    chomp($line);
    ($chr, $arm, $band, $iscntop, $iscnbot, $start, $end, $stain, $density, @rest) = split("\t",$line); 
    # ignore bogus line at the beginning of each chrom
    if (($start == 0) && ($end == 1)) {
	$prevend=0;
	next;
    }
    # Make data 0-based
    if (($start > 0) && ($start > $prevend)) {
	$start--;
    }
    # the first band on a chrom says it starts at '2' instead of '1'
    # we want zero for that
    $start = 0 if (1 == $start);
    # Checks for observed errors in NCBI file
    if ($start == $prevstart) {
	$start = $prevend + 1;
    }
    if (!$stain) {
	$stain = "gneg";
    }
    # Only print lines with band data
    if ($chr =~ /[0-9,X,Y]/) {
	$band =~ s/\://;
	print OUT "chr$chr\t$start\t$end\t$arm$band\t$stain$density\n";
    }
    $prevstart = $start;
    $prevend = $end;
}
