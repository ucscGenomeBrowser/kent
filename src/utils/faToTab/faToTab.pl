#!/usr/bin/env perl

#	$Id: faToTab.pl,v 1.2 2007/08/07 16:27:05 hiram Exp $

use strict;
use warnings;

sub usage()
{
printf STDERR "faToTab.pl - convert fa file to tab separated file\n";
printf STDERR "usage: faToTab.pl <exclude list file> <fasta file>\n";
printf STDERR "examples:\n";
printf STDERR "faToTab.pl exclude.nameList ensGene.fa > ensGene.fa.tab\n";
printf STDERR "zcat ensGene.fa.gz | faToTab.pl /dev/null /dev/stdin > ensGene.fa.tab\n";
}

my $argc = scalar(@ARGV);

if ($argc != 2) { usage; exit 255; }

my $excludeFile = shift;
my %excludeList;

open (FH,"<$excludeFile") or die "can not open $excludeFile: $!";
while (my $line=<FH>)
{
chomp $line;
$excludeList{$line} = 1;
}
close (FH);

my $inputFasta = shift;
open (FH,"<$inputFasta") or die "can not open $inputFasta: $!";

my $excludeThisOne = 0;
my $eol="";		# do not need a <CR> on the first one.

while (my $line = <FH>) {
    chomp $line;
    next if (length($line) < 1);
    if ($line =~ m/^>/) {
	$line =~ s/>//;
	if (exists($excludeList{$line}))
	    {
	    $excludeThisOne = 1;
	    }
	else
	    {
	    $excludeThisOne = 0;
	    }
	if ($excludeThisOne == 0) { printf "%s%s\t", $eol, $line; $eol="\n"; }
    } else {
	if ($excludeThisOne == 0) { printf "%s", $line; }
    }
}
close (FH);
