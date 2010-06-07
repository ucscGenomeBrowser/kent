#!/usr/bin/env perl

#	$Id: pslSplitLineCount.pl,v 1.1 2007/08/06 21:35:03 hiram Exp $

use strict;
use warnings;
use File::Basename;

sub usage() {
    print STDERR "usage: pslSplitLineCount.pl <count> <file.psl> <outRoot>\n";
    print STDERR "\t<count> - number of resulting psl lines in output files\n";
    print STDERR "\t<file.psl> - file to be split into pieces, must have psl header\n";
    print STDERR "\t<outRoot> - prefix for split file names, can include directory\n";
    print STDERR "\tthe resulting split files all have the psl header lines\n";
}

my $argc = scalar(@ARGV);

if ($argc != 3) { usage; exit 255; }

my $count = shift;
my $file = shift;
my $outRoot = shift;

my $rootDir = dirname($outRoot);
my $rootPrefix = basename($outRoot);

my $header = "psLayout version 3

match	mis- 	rep. 	N's	Q gap	Q gap	T gap	T gap	strand	Q        	Q   	Q    	Q  	T        	T   	T    	T  	block	blockSizes 	qStarts	 tStarts
     	match	match	   	count	bases	count	bases	      	name     	size	start	end	name     	size	start	end	count
---------------------------------------------------------------------------------------------------------------------------------------------------------------
";

print $header;

print "dir: $rootDir\n";
print "prefix: $rootPrefix\n";

if ($rootDir ne ".") { print STDERR "mkdir $rootDir\n"; mkdir $rootDir; }

my $fc = 0;
my $linesOut = 0;
my $outFile = "$rootDir/${rootPrefix}_$fc.psl";
open(OUT, ">$outFile") or die "can not write to $outFile";
print STDERR "$outFile\n";
$fc++;
## the initial file already has a header which gets into the first file
## print OUT $header;
open (FH, "<$file") or die "can not read $file";

while (my $line = <FH>) {
    if ($linesOut >= $count) {
	close(OUT);
	$outFile = "$rootDir/${rootPrefix}_$fc.psl";
	open(OUT, ">$outFile") or die "can not write to $outFile";
	print STDERR "$outFile\n";
	$fc++;
	print OUT $header;
	$linesOut = 0;
    }
    print OUT $line;
    $linesOut++;
}
close (OUT);

close (FH);

exit 0;
