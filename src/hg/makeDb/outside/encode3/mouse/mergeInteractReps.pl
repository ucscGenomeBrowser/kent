# Merge Ren lab interaction replicates

# usage: mergeReps rep1.bed rep2.bed

# input file format
# chrom   start   end     ensembl symbol  SCC     Z       p-value (z)     p-value (empirical)
# chr1    4426300 4428300 ENSMUSG00000025902.9    Sox17   6.16E-01        2.07E+00        0.0189990250.016378526

# use max SCC and min p-value of two reps

# output format
# chrom   start   end     ensembl symbol  SCC     p-value       count (1 or 2)

use strict;
use List::Util qw(min max);

die "usage: rep1-file rep2-file\n" if scalar @ARGV != 2;
my ($rep1, $rep2) = @ARGV;

# open files and strip header line
open(my $fh1, $rep1) or die "Can't open file $rep1\n"; <$fh1>;
open(my $fh2, $rep2) or die "Can't open file $rep2\n"; <$fh2>;

# hash up interactions
my %counts;
my %scores;
my %pvals;

# read rep1
while (<$fh1>) {
    chomp;
    my ($chrom, $start, $end, $ensGene, $geneName, $scc, $zscore, $pval) = split;
    my $inter = "$chrom|$start|$end|$ensGene|$geneName";
    $counts{$inter} = 1;
    $scores{$inter} = $scc;
    $pvals{$inter} = $pval;
}

# read rep2 and incorporate
while (<$fh2>) {
    chomp;
    my ($chrom, $start, $end, $ensGene, $geneName, $scc, $zscore, $pval) = split;
    my $inter = "$chrom|$start|$end|$ensGene|$geneName";
    if (exists $counts{$inter}) {
        $counts{$inter} = 2;
        $scores{$inter} = max($scores{$inter}, $scc);
        $pvals{$inter} = min($scores{$inter}, $pval);
    } else {
        $counts{$inter} = 1;
        $scores{$inter} = $scc;
        $pvals{$inter} = $pval;
    }
}
# output merged interactions
while (my ($inter, $count) = each %counts) {
    my $score = $scores{$inter};
    my $pval = $pvals{$inter};
    my ($chrom, $start, $end, $ensGene, $geneName) = split('\|', $inter);
    print "$chrom\t$start\t$end\t$ensGene\t$geneName\t$score\t$pval\t$count\n";
}


