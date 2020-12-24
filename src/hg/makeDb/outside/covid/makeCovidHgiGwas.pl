# Format files from COVID Host Genetics Initiative as BED 9+11 (covidHgiGwas.as) for lollipop display

# Usage: makeCovidHgiGwas.pl <db> <#studies> <input file> [R4] [neg-threshold pos-threshold]

# R4 indicates newer format of input files.  effect | pvalue are alternative views
# thresholds are for effect size coloring when using pvalue view
# effect size view is hard-coded to pvalue 5 threshold (conventional threshold for GWAS)

use strict;
use English;

my $db = $ARGV[0];
my $allStudies = $ARGV[1];
my $file = $ARGV[2];

# support slightly changed format in newer releases
my $version = "R3";     # initial track
if ($#ARGV >= 3) {
    $version = $ARGV[3];
}

# color light/bright threshold
my $colorBy = "pvalue";
my $negThreshold = "5";  # if coloring by -log10 pvalue
my $posThreshold = "5";  # if coloring by -log10 pvalue
if ($#ARGV >=4) {
    $colorBy = "effect";
    $negThreshold = $ARGV[4];
    $posThreshold = $ARGV[5];
}

open(my $fh, $file) or die ("can't open file $file\n");

my $hdr = <$fh>;
chomp($hdr);
my @hdr = split('\t', $hdr);
my $fields = $#hdr + 1;

# skip over middle fields in R3 format (per-study values)

# extended format in R3 hg19 which was lifted from hg38 analysis (4 fields w/ hg38)
if ($db eq "hg38" or $version ne "R3") {
    $fields += 4;
}

# skip field 5 (long name)
my $first = $fields-12;
my $last = $fields-5;

#die 'fields: ' . $fields . ' $first: ' . $first . ' $last: ' . $last . "\n";

my $scale = 3;
my $sizeBins = 5;
while (<$fh>) {
    # parse and generate field values
    chomp;
    my ($chromNum, $pos, $ref, $alt) = split;
    # drop 'chr23' entries (till we identify)
    next if $chromNum eq "23";
    my @data = split;
    my ($studies, $effectSize, $effectSE, $pval, $pvalHet, 
        $samples, $alleleFreq, $snp) = @data[$first..$last];
    my $chr = "chr" . $chromNum;
    my $start = $pos - 1;
    my $end = $pos;
    my $score = 0;
    my $name = ($snp eq "NA") ? $chromNum . ":" . $pos : $snp;
    my $studyWeight = int(($studies * $sizeBins) / $allStudies) + $scale; 

    # assign color
    my $color;
    my $blue = "0,0,255"; my $red = "255,0,0";
    my $lightBlue = "160,160,255"; my $lightRed = "255,160,160";
    my $logPval = -(log($pval)/log(10));
    my $absEffectSize = abs($effectSize);
    my $colorThresholdVal = int($logPval);
    if ($colorBy eq "effect") {
        $colorThresholdVal = $absEffectSize;
    }
    if ($effectSize > 0) {
        # positive (red)
        $color = ($colorThresholdVal >= $posThreshold) ? $red : $lightRed;
    } else {
        # negative (blue)
        $color = ($colorThresholdVal >= $negThreshold) ? $blue : $lightBlue;
    }
    # print
    $OFS = "\t"; print $chr, $start, $end, $name, $score, '.', $start, $end, $color; $OFS = "";
    printf("\t%.3f\t%.3f", $effectSize, $effectSE);
    printf("\t%.2e\t%.3f\t%.2e", $pval, $logPval, $pvalHet);
    printf("\t%s\t%s\t%.3f\t%s\t%s\t%d\t%.3f", $ref, $alt, $alleleFreq, $samples, $studies, 
                $studyWeight, $absEffectSize);
    print "\n";
}
close ($fh);

