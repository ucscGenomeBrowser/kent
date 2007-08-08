#!/usr/bin/env perl

##	$Id: UniSTSParse.pl,v 1.1 2007/08/06 22:13:30 hiram Exp $

#	UniSTSParse.pl - find all mouse marker aliases
#
#	usage: UniSTSParse.pl UniSTS_mouse.sts UniSTS.aliases
#
#	the first column of UniSTS_mouse.sts is its UniSTS ID number
#	using those ID numbers, look up the same line in UniSTS.aliases
#	to get the aliases for that UniSTS ID number
#
#	This is going to fetch the IDs from the .sts file
#	And then the IDs from the .aliases file
#	It will compare the two to make sure there is a correspondence
#	from the .sts ID to an ID in the .aliases

use warnings;
use strict;

#	typical lines from UniSTS_mouse.sts

# 6674	AATTGGCAGCTGCTTAGATG	TGCACGCGTTAAGTTACTTC	215	U23883	-	U23883	Mus musculus
# 9610	ACGAATGTTCCAAGGAGCTA	CATAGTCTCCTAAGTGAACAG	155	U23902	16	U23902	Mus musculus
# 9852	ACTAAAAGTTTGGCGAAACCT	CCAAGAAGCCACTGGACAGGC	102	G36387	-	G36387	Mus musculus

#	typical lines from UniSTS.aliases

# #Unique ID	Aliases
# 1	G31614;SHGC-25164;RH43099
# 2	D3S2404;G08302;GGAA6B07;CHLC.GGAA6B07;CHLC.GGAA6B07.P10163;G00-364-386

my $argc = @ARGV;	#	argument count

if ($argc != 2) {
	print "usage: $0 UniSTS_mouse.sts UniSTS.aliases\n";
	exit 255;
}

my $stsFile = shift;
my $aliasFile = shift;

open (STS,"<$stsFile") or die "Can not open $stsFile";
open (ALIAS,"<$aliasFile") or die "Can not open $aliasFile";

my @stsSeqID;
my %checkIDs;
my $IDCount = 0;
my $lineCount = 0;

#	fetch the IDs from column one of the .sts file, keep in the
#	array stsSeqID[] - there used to be a check for duplicate IDs in
#	the list, but that check fails as there is a duplicate ID in the
#	list.  It happens to have the same sequence
while (my $line = <STS>)
    {
    chomp ($line);
    ++$lineCount;
    next if ($line =~ m/\s*#/);
    my @a = split('\t',$line);
    if (exists $checkIDs{$a[0]})
	{
	if ($checkIDs{$a[0]} != 108991) {
	print "#\tWARNING: KNOWN(==OK) duplicate ID: '$a[0]' ",
		"encountered at line ", "$lineCount\n";
	} else {
	print "#\tERROR: duplicate ID: '$a[0]' encountered at line ",
		"$lineCount\n";
	}
	} else {
	$checkIDs{$a[0]} = 1;
	$stsSeqID[$IDCount++] = $a[0];
	}
    }

print "#\tread $IDCount mouse STS IDs from $stsFile\n";
close (STS);

my %mouseAlias;
my $aliasCount = 0;
$lineCount = 0;

#	Now fetch the IDs from the .aliases file
#	As of 2005-04-06 there appears to be a change in the format of
#	this file.  It seems to have multiple ID numbers on single lines
#	as if each ID number applies to the aliases at the end of the
#	line.  But then in has one long line at the end of the file with
#	a whole bunch of ID numbers, but no alias ?
#	And, as of 2005-09-29, the names can have blanks in them.
while (my $line = <ALIAS>)
    {
    chomp ($line);
    ++$lineCount;
    next if ($line =~ m/\s*#/);
    my @a = split('\t',$line);
    if (scalar(@a) > 2) {
	print "#\tERROR: multiple tabbed fields at line $lineCount",
		" in alias file $aliasFile\n"
    }
    my $aliasStringIndex = scalar(@a) - 1;
    for (my $i = 0; $i < $aliasStringIndex; ++$i)
	{
	if (exists $mouseAlias{$a[$i]})
	    {
	    print "#\tERROR: duplicate ID at line $lineCount in alias ",
		    "file $aliasFile\n";
	    } else {
	    $mouseAlias{$a[$i]} = $a[$aliasStringIndex];
	    ++$aliasCount;
	    }
	}
    }

print "#\tread $aliasCount alias IDs from $aliasFile\n";
close (ALIAS);

#	The resulting output is each alias that has a corresponding STS
#	sequence for the same ID.  Thus, unused aliases are trimmed from
#	the .aliases file.
#
#	If an STS sequence exists, but it has no alias, then it has
#	no alias, it's "alias" is thus it's name
for (my $i = 0; $i < $IDCount; ++$i)
    {
    if (! exists($mouseAlias{$stsSeqID[$i]}))
	{
	print "#\talias for $stsSeqID[$i] is itself, has no alias\n";
	print "$stsSeqID[$i]\t$stsSeqID[$i]\n";
	} else {
	print "$stsSeqID[$i]\t$mouseAlias{$stsSeqID[$i]}\n";
	}
    }
