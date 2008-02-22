#!/usr/bin/env perl

# Combine Ensembl MySQL tables seq_region and assembly to produce
#	a lift specification for GeneScaffold to scaffold coordinates.

# $Id: ensAssemblyChain.pl,v 1.1 2008/02/22 20:00:45 hiram Exp $

use strict;
use warnings;

my $argc = scalar(@ARGV);

if (2 != $argc) {
    print STDERR "usage: ./ensAssemblyChain.pl mysql/seq_region.txt.gz ",
	"mysql/assembly.txt.gz > oryCun1.ensGene.lft\n";
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
my $chainCount = 0;
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
    ++$chainCount;
    my $strand="+";
    $strand = "-" if ($ori < 0);
    if ($asmName =~ m/^GeneScaffold_/ && $cmpName =~ m/^scaffold_/) {
	printf "chain 100 %s %s + %s %s %s %s %s %s %s %s\n",
	    $asmName, $asmLength, $asmStart-1, $asmEnd,
		$cmpName, $cmpLength, $strand, $cmpStart-1, $cmpEnd,
		    $chainCount;
        printf "%d 0 0\n", $checkLength;
        print "0\n\n";
    }
}
close (FH);
