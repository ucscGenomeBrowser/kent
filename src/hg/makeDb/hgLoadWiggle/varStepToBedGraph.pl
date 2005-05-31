#!/usr/bin/env perl
#
#	varStepToBedGraph.pl - take variableStep wiggle data (span=1 only)
#		and convert it to bedGraph bed-like format.

#	$Id: varStepToBedGraph.pl,v 1.1 2005/05/31 22:47:56 hiram Exp $

use warnings;
use strict;

sub usage()
{
my $bn=`basename $0`;
chomp $bn;
printf "usage: %s <variable step format file names>\n", $bn;
exit 255;
}
my $argc = scalar(@ARGV);	#	arg count

usage if ($argc < 1);

my $linesRead = 0;
my $dataLines = 0;
my $variableDeclarations = 0;
my $chrom="";
my $span=1;
my $itemNumber = 0;

while (my $file=shift)
    {
    open (FH,"<$file") or die "Can not open $file";
    while (my $line=<FH>)
	{
	++$linesRead;
	next if ($line =~ m/^\s*#/);
	next if ($line =~ m/^track\s+/);
	chomp $line;
	if ($line =~ m/^variableStep/)
	    {
	    $span=1;
	    (my @a) = split('\s+',$line);
	    for (my $i = 0; $i < scalar(@a); ++$i)
	    {
	    if ($a[$i] =~ m/^chrom=/)
		{
		(my $dummy, $chrom) = split('=',$a[$i]);
		}
	    elsif ($a[$i] =~ m/^span=/)
		{
		(my $dummy, $span) = split('=',$a[$i]);
		}
	    elsif ($a[$i] =~ m/^variableStep/)
		{
		next;
		}
	    else
		{
		printf STDERR "ERROR: illegal field %s in variableStep declaration at line # %d\n", $a[$i], $linesRead;
		printf STDERR "'$line'\n";
		exit 255;
		}
	    }
	    if ($span != 1)
		{
		printf STDERR "ERROR: span not 1 - not handling other spans, at line # %d\n", $linesRead;
		printf STDERR "'$line'\n";
		exit 255;
		}
	    ++$variableDeclarations;
	    }
	    else
	    {
	    my ($position, $value) = split('\s+',$line);
	    ++$dataLines;
	    printf "%s\t%d\t%d\t%s\n", $chrom, $position-1, $position, $value;
	    ++$itemNumber;
	    }
	}
    close (FH);
    }

printf STDERR "Processed %d lines input, %d data lines, %d variable step declarations\n", $linesRead, $dataLines, $variableDeclarations;
