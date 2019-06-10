#!/usr/bin/env perl

# factorSourceCheckScore.pl - report if score is not max of scores in sourceScores
#    verifying fix for RM #13224

use English;
use feature 'say';
use strict;
use autodie;

$OUTPUT_AUTOFLUSH = 1;  # flush stdout (log) after every print

my $lines = 0;
my $errors = 0;
while (<>) {
    $lines += 1;
    my ($chrom, $start, $end, $name, $score, $sourceCount, $sourceIds, $sourceScores) = split;
    my @values = split(',', $sourceScores);
    my $max = (sort { $b <=> $a } @values)[0];
    if ($score != $max) {
        say "line $lines: $score, $max";
        $errors += 1;
    }
}
say "Errors: $errors in $lines lines";
    
