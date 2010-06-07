#!/usr/bin/env perl

#	ucscAlias.pl - extract the UCSC stsAlias three column file
#		formate from the stsMapMouse.bed file
#
# $Id: ucscAlias.pl,v 1.1 2006/09/09 23:38:36 hiram Exp $

use strict;
use warnings;

my $argc = @ARGV;	#	arg count

if ($argc != 1) {
	print "usage: $0 stsInfoMouse.bed > ucscStsAlias.txt\n";
	print "\tload the ucscStsAlias.txt file for the stsAlias table data\n";
	exit 255;
}


#	typical line in stsInfoMouse.bed file

# 1	U23883	106145	D16IUM41	6674	2	U23883;MMU23883	AATTGGCAGCTGCTTAGATG	TGCACGCGTTAAGTTACTTC	215	0	MUS MUSCULUS				MGD	16	57.6
# 2	U23902	106124	D16IUM60	9610	5	D16IUM60;U23902;MGI:106124;MMU23902;MGI:7872	ACGAATGTTCCAAGGAGCTA	CATAGTCTCCTAAGTGAACAG	155	0	MUS MUSCULUS				MGD	16	38.80

my $fName = shift;

open (FH, "<$fName") or die "Can not open $fName";

my $lineCount = 0;

while (my $line = <FH>) {
    chomp $line;
    ++$lineCount;
    my @a = split('\s',$line);
    my $ucscID = $a[0];
    my $trueName = $a[1];
    if (length($ucscID) < 1) {
print STDERR "#\tERROR: no ucscID at line $lineCount\n";
    } else {
	if (length($trueName) < 1) {
print STDERR "#\tERROR: no trueName at line $lineCount\n";
	} else {
	    if (length($a[6]) < 1) {
		print STDERR "#\tWARNING: no aliases found for ucscID: $ucscID and trueName: $trueName\n";
		print "$trueName\t$ucscID\t$trueName\n";
	    } else {
		my @b = split(';',$a[6]);
		my $bCount = @b;
		for (my $i = 0; $i < $bCount; ++$i) {
			print "$b[$i]\t$ucscID\t$trueName\n";
		}
	    }
	} 
    } 
}
close (FH);
