#!/usr/bin/env perl

#	$Id: sortRandoms.pl,v 1.1 2007/07/19 20:26:38 hiram Exp $

use strict;
use warnings;

sub usage() {
    print STDERR "usage: ./sortRandoms.pl randomContigs.agp\n";
    print STDERR "\textracts contig names from randomContigs.agp\n";
    print STDERR "\tthen moves that contig.fa file from\n";
    print STDERR "\tsplitContigs/chrN/contig.fa to splitContigs/chrN_random/contig.fa\n";
}

my $argc = scalar(@ARGV);

if ($argc != 1) { usage; exit 255; }

my $rcFile = shift;

open (FH,"$rcFile") or die "can not open $rcFile";

while (my $line = <FH>) {
    my ($chrRandom, $start, $stop, $id, $type, $name, $ctgStart, $yesNo) =
	split('\t',$line,8);
    next if ($ctgStart =~ m/contig/);
    my $C = $chrRandom;
    $C =~ s/_random//;
    my $srcFile = "splitContigs/$C/$name.fa";
    my $dstDir = "splitContigs/${C}_random";
    die "can not find src: $srcFile" if (! -e $srcFile);
    print "mkdir -p $dstDir\n";
    print "mv $srcFile $dstDir/$name.fa\n";
}

close (FH);

