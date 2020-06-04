# Add fields to ENCODE DAC ccre for UCSC display

use strict;
use English;

while (<>) {
    chomp;
    my ($chrom, $start, $end, $name, $score, $strand, 
                $tstart, $tend, $color, $ccre, $zscore) = split;
    my $accessionLabel = $name;

    # strip assembly prefix for more readable item name
    $accessionLabel =~ s/^EH38//;
    $accessionLabel =~ s/^EM10//;

    # combine and assign more readable labels to ccre groups
    my $ucscLabel = "";
    my $elementDescr = "";
    my $group = $ccre;
    $group =~ s/,CTCF-bound//;  # remove trailing notation from labels for readability
    if ($group eq 'PLS') {
       $ucscLabel = 'prom'; 
       $elementDescr = 'promoter-like signature'
    } elsif ($group eq 'pELS') {
       $ucscLabel = 'enhP'; 
       $elementDescr = 'proximal enhancer-like signature'
    } elsif ($group eq 'dELS') {
       $ucscLabel = 'enhD'; 
       $elementDescr = 'distal enhancer-like signature'
    } elsif ($group eq 'CTCF-only') {
       $ucscLabel = 'CTCF'; 
       $elementDescr = 'CTCF-only'; 
    } elsif ($group eq 'DNase-H3K4me3') {
       $ucscLabel = 'K4m3'; 
       $elementDescr = 'DNase-H3K4me3'; 
    } else {
       $ucscLabel = $ccre;
       $elementDescr = $ccre;
    }
    my $mouseOver = "$name $elementDescr";
    $OFS = "\t";
    print $chrom, $start, $end, $name, $score, $strand, $tstart, $tend, $color, 
                $ccre, $group, $zscore, $ucscLabel, $accessionLabel, $mouseOver . "\n";
}

