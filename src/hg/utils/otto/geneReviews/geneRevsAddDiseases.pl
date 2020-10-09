#!/usr/bin/env perl

# geneReviewsAddDiseases.pl diseaseFile bedFile
#
# To support mouseOver with disease names via adding disease list to BED file
# RM #19841
#
# Parse gene disease file created by extendGeneReviews.pl
#       <gene> <disease1>;<disease2>;... <diseaseN>
#
# Add last column containing disease list to each per-gene BED row in bed file
# and write to standard output

use strict;
use English;
use feature 'say'; # append newline when printing

my ($diseaseFile, $bedFile) = @ARGV;

# Read in gene/disease file

my %geneDiseases;
my %geneDiseaseCounts;
open(my $D, $diseaseFile) or die ("can't open file $diseaseFile\n");
while (<$D>) {
    chomp;
    my ($gene, $count, $diseases) = split("\t");
    #print "DEBUG:  ", $gene, "\t", $count, "\t",$diseases, "\n";
    $geneDiseases{$gene} = $diseases;
    $geneDiseaseCounts{$gene} = $count;
    #print "DEBUG:  ", $gene, ": ", $geneDiseases{$gene}, "\n";
}
close $D;

# Read stdin bed file and append diseases to each row

$OFS="\t"; 
open(my $B, $bedFile) or die ("can't open file $bedFile\n");
while (<$B>) {
    chomp;
    my ($chrom, $start, $end, $gene) = split("\t");
    #print "DEBUG: ", "\t", $gene, "\n"; 
    my $diseases = "n/a";
    my $count = 0;
    if (exists($geneDiseases{$gene})) {
        $diseases = $geneDiseases{$gene};
        $count = $geneDiseaseCounts{$gene};
    }
    say $chrom, $start, $end, $gene, "0", ".", $start, $end, 0, $count, $diseases;
}

