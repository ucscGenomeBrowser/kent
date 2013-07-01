#!/usr/bin/env perl

# Combine Ensembl MySQL tables seq_region and assembly to produce
#	a lift specification for GeneScaffold to scaffold coordinates.

# $Id: ensGeneScaffolds.pl,v 1.1 2008/02/22 22:51:28 hiram Exp $

use strict;
use warnings;

my $argc = scalar(@ARGV);

if (2 != $argc) {
    print STDERR "usage: ensGeneScaffolds.pl ",
	"../download/seq_region.txt.gz \\\n",
	"\t../download/assembly.txt.gz > ensGene.lft\n";
    print STDERR "\nCombine Ensembl MySQL tables 'seq_region'\n",
	"and 'assembly' to produce a lift specification for GeneScaffold\n",
	"to scaffold coordinates.\n";
    exit 255;
}

my $seqRegion = shift;
my $assembly = shift;

my %region;
my $regionCount = 0;

open (FH,"zcat $seqRegion|") or die "can not zcat $seqRegion";
while (my $line = <FH>) {
    my ($id, $name, $coordSystem, $length) = split('\s+', $line);
    $region{$id} = "$name|$length";
    ++$regionCount;
}
close (FH);

my %assem;
my $assemCount = 0;
open(FH,"zcat $assembly|") or die "can not zcat $assembly";
while (my $line = <FH>) {
    my ($asmId, $cmpId, $asmStart, $asmEnd, $cmpStart, $cmpEnd, $ori) =
	split('\s+', $line);
    die "can not find asm seq_region id: $asmId" if (!exists($region{$asmId}));
    die "can not find cmp seq_region id: $cmpId" if (!exists($region{$cmpId}));
    my ($cmpName, $cmpLength) = split('\|', $region{$cmpId});
    my ($asmName, $asmLength) = split('\|', $region{$asmId});
    my $partLength = $cmpEnd - $cmpStart + 1;
    my $checkLength = $asmEnd - $asmStart + 1;
    die "assembled length $checkLength != component length $partLength"
	if ($partLength != $checkLength);
    my $strand = "+";
    $strand = "-" if ($ori < 0);
    if ($asmName =~ m/^GeneScaffold_/ && (($cmpName =~ m/^scaffold_/) || ($cmpName =~ m/^Scaffold/) || ($cmpName =~ m/^contig/)) ) {
	printf "%s\t%s\t%s\t%s\t%s\t%s\n", $asmName, $asmStart-1, $asmEnd,
		$cmpName, $cmpStart-1, $strand;
    }
    ++$assemCount;
}
close (FH);

print STDERR "have $assemCount assembly lines\n";
