#!/usr/bin/perl -w
use strict;

# File: distNcbiCtgFa
# Author: Terry Furey
# Date: 10/2001
# Description:  Distributes NT_XXXXX contig fa files to appropriate 
# directory within the assembly directory structure.  Assumes that 
# fa's are all contained within a single directory and that lift
# files have been created
# 2004-05-20 - Update chroms listing, made strict safe - Hiram

# Usage message
if ($#ARGV != 1) {
    print STDERR "USAGE: distNcbiCtgFa <fa dir> <ctg dir>\n";
    exit(1);
}

# Read parameters
my $fa = shift(@ARGV);
if (!-e "$fa") { die("$fa does not exist\n"); }
my $ctgdir = shift(@ARGV);
if (!-e "$ctgdir") { die("$ctgdir does not exist\n"); }

# Process each chromosome
#	@chroms = (1..22,X,Y,qw(Un));
my @chroms = (1 .. 22, 'X', 'Y', 'M', '6_hla_hap1', '6_hla_hap2');
foreach my $chr (@chroms) {
    if (!-e "$ctgdir/$chr") {
	die("$ctgdir/$chr does not exist\n");
    }
    if (!-e "$ctgdir/$chr/lift") {
	die("$ctgdir/$chr/lift does not exist\n");
    }
    open(FILE, "<$ctgdir/$chr/lift/ordered.lft") or
	die "Can not open $ctgdir/$chr/lift/ordered.lft";
    while (my $line = <FILE>) {
	chomp($line);
	(my $start, my $thisctg, my $size, my $thischr, my $total) =
		split(' ',$line);
	(my $slop, my $ctg) = split(/\//,$thisctg);
	if (!-e "$fa/${ctg}.fa") {
	    die("$fa/${ctg}.fa does not exist\n");
	} else {
	    if (!-e "$ctgdir/$chr/$ctg") {
		`mkdir $ctgdir/$chr/$ctg`;
	    }
	    `mv $fa/${ctg}.fa $ctgdir/$chr/$ctg/${ctg}.fa`;
	}
    }
    close(FILE);
    if (open(FILE, "<$ctgdir/$chr/lift/random.lft"))
	{
	while (my $line = <FILE>)
	    {
	    chomp($line);
	    (my $start, my $thisctg, my $size, my $thischr, my $total) =
		    split(' ',$line);
	    (my $slop, my $ctg) = split(/\//,$thisctg);
	    if (!-e "$fa/${ctg}.fa")
		{
		die("$fa/${ctg}.fa does not exist\n");
		} else {
		    if (!-e "$ctgdir/$chr/$ctg") {
			`mkdir $ctgdir/$chr/$ctg`;
		    }
		`mv $fa/${ctg}.fa $ctgdir/$chr/$ctg/${ctg}.fa`;
		}
	    }
	}
    close(FILE);
}
