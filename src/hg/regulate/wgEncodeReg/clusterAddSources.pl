#!/usr/bin/env perl

# clusterAddSources - create clusters file that includes input sources
#
# Created to provide a user-friendly download file for clusters displayed in
# UCSC factorSource type tracks.
# The input source names (e.g. cell type) are extracted from the inputs file for the track.
# The inputs file format uses column 2 for the source identifier.
# The clusters file is UCSC BED15 format (see genome.ucsc.edu/FAQ/FAQformat.html);

use warnings;
use strict;

# Set this option to 1 to print full source info (metadata terms separated by '+')
# Default prints just the cell type, and removes duplicates
my $baseOpt = 0;

scalar(@ARGV) == 2 or die "usage: clusterAddSources clusters.bed inputs.tab\n";

my $clusterFile = $ARGV[0];
my $inputFile = $ARGV[1];
my @inputs;

# read inputs 
open(IF, $inputFile) or die "can't read $inputFile\n";
while (<IF>) {
    my ($obj, $source) = split('\t');
    $source =~ s/\+.*// if ($baseOpt != 0);
    push(@inputs, $source);
}

# read clusters
open(CF, $clusterFile) or die "can't read $clusterFile\n";
while (<CF>) {
    chomp;
    my ($chrom, $start, $end, $name, $score) = split('\t');
    my @fields = split("\t");
    my @expScores = split(",", $fields[14]);
    my @sources;
    my %sources = ();
    my $i = -1;

    # grab experiments with non-zero scores and look up source
    foreach my $expScore (@expScores) {
        $i++;
        next if (!$expScore);
        my $input = $inputs[$i];
        if ($baseOpt) {
            next if (defined($sources{$input}));
            $sources{$input} = 1;
        }
        push(@sources, $input);
    }
    my $sources = join(',', @sources);

    # output BED 5+ line
    print join("\t", $chrom, $start, $end, $name, $score, $sources), "\n";
}
