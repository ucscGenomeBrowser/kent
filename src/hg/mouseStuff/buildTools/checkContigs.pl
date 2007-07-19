#!/usr/bin/env perl

#	$Id: checkContigs.pl,v 1.1 2007/07/19 20:26:38 hiram Exp $

use strict;
use warnings;

sub usage() {
    print STDERR "usage: ./checkContigs.pl allContigs.agp\n";
    print STDERR "\textracts contig names from allContigs.agp\n";
    print STDERR "\tand verfies they exist in splitContigs/chrN/contig.fa\n";
}

my $argc = scalar(@ARGV);

if ($argc != 1) { usage; exit 255; }

my $rcFile = shift;

open (FH,"$rcFile") or die "can not open $rcFile";

while (my $line = <FH>) {
    my ($chrN, $start, $stop, $id, $type, $name, $ctgStart, $yesNo) =
	split('\t',$line,8);
    next if ($ctgStart =~ m/contig/);
    my $C = $chrN;
    my $srcFile = "splitContigs/$C/$name.fa";
    die "can not find src: $srcFile" if (! -e $srcFile);
    print "$srcFile\n";
}

close (FH);

