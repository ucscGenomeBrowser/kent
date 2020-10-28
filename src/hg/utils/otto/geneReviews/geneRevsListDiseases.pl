#!/usr/bin/env perl

# geneRevGeneDiseases.pl

# Args
#       gene file:  id, shortDisease, gene
#       disease file: shortDisease, title, id, PMID
#
# Parse gene and disease files pre-processed by geneReviewsBuild.sh and
# Create intermediate file to standard out, with disease list per gene:
#   <gene>    <disease1; disease2; ... diseaseN>
#
# Semi-colon is separator since GeneReviews includes commas in disease titles

# To support mouseOver with disease names via adding disease list to BED file
# RM #19841

use strict;
use English;

# separator for output string
my $sep = "; ";

my ($geneFile, $diseaseFile) = @ARGV;

# read disease file, map short name to title
my %shortToTitle;
open(my $D, $diseaseFile) or die ("can't open file $diseaseFile\n");
while (<$D>) {
    chomp;
    my ($short, $title, $id, $pmid) = split("\t");
    #print "DEBUG:  ", $short, "\t", $title, "\n";
    $shortToTitle{$short} = $title;
    #print "DEBUG:  ", $short, ": ", $shortToTitle{$short}, "\n";
}

# read gene file, create disease list per gene
my %genes;
open(my $G, $geneFile) or die ("can't open file $geneFile\n");
while (<$G>) {
    chomp;
    my ($id, $short, $gene) = split("\t");;
    #print "DEBUG:  ", $gene, "\t", $short, "\n";
    my $diseases = "";
    if (exists($genes{$gene})) {
        $diseases = $genes{$gene} . $sep;
    }
    $genes{$gene} = $diseases . $shortToTitle{$short};
}

#print "DEBUG: ", keys %genes, "\n";

foreach my $gene (keys %genes) {
    #print $gene . "\t" . $genes{$gene};
    my @diseases = $genes{$gene} =~ /$sep/g;
    my $count = @diseases + 1;
    #print "\t" . $count . "\n";
    print $gene . "\t" . $count . "\t" . $genes{$gene} . "\n";
}

