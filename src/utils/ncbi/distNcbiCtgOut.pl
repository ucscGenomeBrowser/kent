#!/usr/bin/perl
# File: distNcbiCtgFa
# Author: Terry Furey
# Modified: Matt Schwartz 4/11/03
# Date: 10/2001
# Description:  Distributes NT_XXXXX contig fa files to appropriate 
# directory within the assembly directory structure.  Assumes that 
# fa's are all contained within a single directory and that lift
# files have been created

# Usage message
if ($#ARGV != 1) {
    print stderr "USAGE: distNcbiCtgOut <fa dir> <ctg dir>\n";
    exit(1);
}

# Read parameters
$fa = shift(@ARGV);
if (!-e "$fa") { die("$fa does not exist\n"); }
$ctgdir = shift(@ARGV);
if (!-e "$ctgdir") { die("$ctg does not exist\n"); }

# Process each chromosome
@chroms = (1..22,X,Y,qw(Un));
foreach $chr (@chroms) {
    if (!-e "$ctgdir/$chr") {
	die("$ctgdir/$chr does not exist\n");
    }
    if (!-e "$ctgdir/$chr/lift") {
	die("$ctgdir/$chr/lift does not exist\n");
    }
    open(FILE, "<$ctgdir/$chr/lift/ordered.lft");
    while ($line = <FILE>) {
	chomp($line);
	($start, $thisctg, $size, $thischr, $total) = split(' ',$line);
	($slop, $ctg) = split(/\//,$thisctg);

	    print "Moving $fa/${ctg}*.* $ctgdir/$chr/$ctg\n";
	    `mv $fa/${ctg}*.* $ctgdir/$chr/$ctg`;
    }
    close(FILE);
    open(FILE, "<$ctgdir/$chr/lift/random.lft");
    while ($line = <FILE>) {
	chomp($line);
	($start, $thisctg, $size, $thischr, $total) = split(' ',$line);
	($slop, $ctg) = split(/\//,$thisctg);

	    print "Moving $fa/${ctg}*.* $ctgdir/$chr/$ctg\n";
	    `mv $fa/${ctg}*.* $ctgdir/$chr/$ctg`;
    }
    close(FILE);
}
