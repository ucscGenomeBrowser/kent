#!/usr/bin/env perl

use strict;
use warnings;

my $argc = scalar(@ARGV);

if ($argc != 1) {
    printf STDERR "usage: ensemblToGeneName.pl infoOut.txt > ensemblToGeneName.tab\n";
    printf STDERR "scans the infoOut.txt file for the columns transId and geneName\n";
    printf STDERR "output is two column tsv: transId geneName\n";
    printf STDERR "to be loaded into the table ensemblToGeneName\n";
    exit 255;
}

my $file = shift;

open (FH, "<$file") or die "can not read $file";
# expecting the first line to be the header
my $header = <FH>;
chomp $header;
$header =~ s/^#//;
my @a = split('\s+', $header);
my $transIdIx = -1;
my $geneNameIx = -1;
for (my $i = 0; $i < scalar(@a); ++$i) {
    if ($a[$i] =~ m/^transId$/) {
	$transIdIx = $i;
    } elsif ($a[$i] =~ m/^geneName$/) {
	$geneNameIx = $i;
    }
}
if ($transIdIx < 0) {
    die "ERROR: can not find transId column in header line";
}

if ($geneNameIx < 0) {
    die "ERROR: can not find geneName column in header line";
}
my $maxCol = $transIdIx > $geneNameIx ? $transIdIx : $geneNameIx;

# printf STDERR "transId at: $transIdIx, geneName at: $geneNameIx\n";

while (my $line = <FH>) {
    chomp $line;
    @a = split('\t', $line);
    my $colCount = scalar(@a);
    if ($colCount > $maxCol && length($a[$transIdIx]) > 0 && length($a[$geneNameIx]) > 0) {
       printf "%s\t%s\n", $a[$transIdIx], $a[$geneNameIx]; 
    }
}
close(FH);
