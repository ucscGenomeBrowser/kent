#!/bin/env perl

use warnings;
use strict;
use File::Basename;

my $argc = scalar(@ARGV);

if ($argc != 2) {
    printf STDERR "usage: ./constructLiftFile.pl chrom.sizes target.list > target.lift\n";
    exit 255;
}

my %chromSize;
my $file = shift;
open (FH, "<$file") or die "can not read $file";
while (my $line = <FH>) {
    chomp $line;
    my ($chr, $size) = split('\s+', $line);
    $chromSize{$chr} = $size;
}
close (FH);

$file = shift;
open (FH, "<$file") or die "can not read $file";
while (my $line = <FH>) {
    chomp $line;
    my $twoBitSpec = basename($line);
    my ($twoBitFile, $chrom, $startEnd) = split(':', $twoBitSpec);
    my ($start, $end) = split('-', $startEnd);
    my $length = $end - $start;
    die "can not find chrom size for $chrom" if (! exists($chromSize{$chrom}));
    my $size = $chromSize{$chrom};
    if (($twoBitSpec =~ m/:0-/) && ($twoBitSpec !~ m/:0-.00.0000/)) {
    printf "%d\t%s\t%d\t%s\t%d\n", $start, $chrom, $length, $chrom, $size;
    } else {
    printf "%d\t%s:%d-%d\t%d\t%s\t%d\n", $start, $chrom, $start, $end, $length, $chrom, $size;
    }
}
close (FH);

__END__
# example line in a target.list file:
/hive/data/genomes/ce9/bed/testLastz/ce9.2bit:chrV:0-10010000
