#!/usr/bin/env perl

# linkPeptideMappings - create BED12+ from individual exon mappings from peptideAtlas

use warnings;
use strict;

scalar(@ARGV) == 1 or die "usage: linkPeptideMappings in.bed\n";

# read in mappings

my %mappings = ();

my $inFile = $ARGV[0];
open(IF, $inFile) or die "can't read $inFile\n"; 
while (<IF>) {
    chomp;
    my ($chrom, $start, $end, $name, $score, $strand, $protein, $ensGene) = split('\t');
    my $key = join("|", $name, $protein, $chrom, $strand, $score);
    if (!defined($mappings{$key})) {
        $mappings{$key} = ();
    }
    my %region = (start =>  $start, end =>  $end);
    push @{$mappings{$key}}, \%region;
}

# link up exons and generate beds

my %beds = ();

foreach my $key (keys %mappings) {
    #print $key, "\n";
    my ($name, $protein, $chrom, $strand, $score) = split('\|', $key);
    my @sortedRegions = sort {$a->{start} <=> $b->{start}} @{$mappings{$key}};
    my $blockCount = scalar @sortedRegions;
    my $chromStart = $sortedRegions[0]->{start};
    my $chromEnd = $sortedRegions[$blockCount-1]->{end};
    my @blockSizes = ();
    my @blockStarts = ();
    for my $region (@sortedRegions) {
        push @blockSizes, $region->{end} - $region->{start};
        push @blockStarts, $region->{start} - $chromStart;
    }
    my $blockSizes = join(",", @blockSizes);
    my $blockStarts = join(",", @blockStarts);
    my $bed = join("\t", $chrom, $chromStart, $chromEnd, $name, $score, $strand,
                                $chromStart, $chromEnd, "0", 
                                $blockCount, $blockSizes, $blockStarts);
    if (!defined($beds{$bed})) {
        $beds{$bed} = 1;
    }
}

# print out unique bed12's

foreach my $key (keys %beds) {
    print $key, "\n";
}

