#!/usr/bin/perl
# File: cpNcbiFaScratch
# Author: Terry Furey
# Date: 10/2001
# Description:  Copy all contig fa's to a single location on
# kkstore /scratch to get ready to propagate to cluster nodes

# Usage message
if ($#ARGV != 1) {
    print stderr "USAGE: cpNcbiFaScratch <src dir> <dest dir>\n";
    exit(1);
}

# Read in and check parameters
$src = shift(@ARGV);
if (!-e "$src") { die("$src does not exist\n"); }
$dest = shift(@ARGV);
if (!-e "$dest") { die("$dest does not exist\n"); }
if (!-e "$dest/chrom") { `mkdir $dest/chrom`; }
if (!-e "$dest/contig") { `mkdir $dest/contig`; }

# Read through all lift files and cp fa files
@chroms = (1..22,X,Y,qw(Un),M);
foreach $chr (@chroms) {
    print stderr "copying chr$chr.fa\n";
    `cp $src/$chr/chr${chr}.fa $dest/chrom/`; 
    if (!-e "$src/$chr/lift") {
	die("$src/$chr/lift does not exist\n");
    }
    if (open(LIFT, "<$src/$chr/lift/ordered.lft")) {
	while ($line = <LIFT>) {
	    chomp($line);
	    ($start, $name, $size, $chrname, $chrsize) = split(" ",$line);
	    (my $thischr,my $ctg) = split(/\//, $name);
	    if (!-e "$src/$chr/$ctg/$ctg.fa") {
		die("$src/$chr/$ctg/$ctg.fa does not exist\n");
	    }
	    print stderr "copying $ctg.fa\n";
	    `cp $src/$chr/$ctg/$ctg.fa $dest/contig`;
	}
	close(LIFT);
    }
    print stderr "copying chr${chr}_random.fa\n";
    `cp $src/$chr/chr${chr}_random.fa $dest/chrom/`; 
    if (open(LIFT, "<$src/$chr/lift/random.lft")) {
	while ($line = <LIFT>) {
	    chomp($line);
	    ($start, $name, $size, $chrname, $chrsize) = split(" ",$line);
	    (my $thischr,my $ctg) = split(/\//, $name);
	    if (!-e "$src/$chr/$ctg/$ctg.fa") {
		die("$src/$chr/$ctg/$ctg.fa does not exist\n");
	    }
	    print stderr "copying $ctg.fa\n";
	    `cp $src/$chr/$ctg/$ctg.fa $dest/contig`;
	}
	close(LIFT);
    }
}
