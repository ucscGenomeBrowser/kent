#!/usr/bin/env perl

##	$Id: stsInfoMouseParse.pl,v 1.1 2007/08/06 22:14:07 hiram Exp $

#	stsInfoMouseParse.pl - fetch all aliases from the stsInfoMouse.bed
#		file.  It verifies the integrity of the file too.


#	format of the stsInfoMouse.bed file
#	the 5th column is the STS ID - the 7th column are the aliases

# 1       U23883  106145  D16IUM41        6674    2       U23883;MMU23883 AATTGGCAGCTGCTTAGATG    TGCACGCGTTAAGTTACTTC    215     0       MUS MUSCULUS           MGD      16      57.6


use warnings;
use strict;

my $argc = @ARGV;	#	arg count

if ($argc != 1)
    {
    print "usage: $0 stsInfoMouse.bed\n";
    exit 255;
    }

my $infoBedFile = shift;

open (FH, "<$infoBedFile") or die "Can not open $infoBedFile";

my %mouseAlias;
my %nameCount;
my %linesRead;
my $aliasCount = 0;
my $lineCount = 0;

while (my $line = <FH>)
    {
    chomp $line;
    ++$lineCount;
    my @a = split('\s',$line);
    my $name = $a[1];
    ++$nameCount{$name};
    if (length($name) < 1)
	{
	print "#\tERROR: null name found for $a[0] at line $lineCount\n";
	print "# '$line'\n";
	}
    if ($nameCount{$name} > 1)
	{
	print "#\tERROR: duplicate name $name found at line $lineCount\n";
	print "# '$line'\n";
	}
    if (exists($mouseAlias{$a[4]}))
	{
	print "#\tERROR: duplicate ID found: '$a[4]' at line $lineCount\n";
	print "# < $linesRead{$a[4]}\n";
	print "# > $line\n";
	} else {
	if (length($a[4]) < 1)
	    {
	    print "#\tERROR: null ID value for $a[0] at line $lineCount for aliases: $a[6]\n";
	    print "# $line\n";
	    } else {
	    if (length($a[6]) < 1)
		{
		print "#\tERROR: null aliases value at line $lineCount for ID: $a[4]\n";
		print "# $line\n";
		} else {
		$linesRead{$a[4]} = $line;
		$mouseAlias{$a[4]} = $a[6];
		++$aliasCount;
		}
	    }
	}
    }
close (FH);

print "#\tread $aliasCount aliases from $infoBedFile\n";
print "#\tname count:\n";
my @sortedNames = sort keys %nameCount;
foreach my $key(@sortedNames)
    {
    print "#\t$key\t$nameCount{$key}\n";
    }

my @sortedKeys = sort keys %mouseAlias;

foreach my $key(@sortedKeys)
    {
    if (length($key) < 1)
	{
	print "#\tERROR: null ID value for $key: $mouseAlias{$key}\n";
	print "# $linesRead{$key}\n";
	} else {
	if (length($mouseAlias{$key}) < 1)
	    {
	    print "#\tERROR: null alias value for ID: $key\n";
	    print "# $linesRead{$key}\n";
	    } else {
		print "$key\t$mouseAlias{$key}\n";
	    }
	}
    }
