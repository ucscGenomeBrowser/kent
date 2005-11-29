#!/usr/bin/perl
# replace "reserved" field of BED 13 with RGB value from 8-scale 
# black->red palette, based on score value

# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/makeColoredBed.pl,v 1.2 2005/11/29 22:08:07 kate Exp $

# palette from Hiram's recommended colors (rgbItemExamples.html)
my @colors = ("34,34,34","68,48,48","102,50,50","136,52,52","170,54,54","204,56,56","238,58,58","255,85,85");

<>; #throw out initial (custom track) line
while (<>) {
    chomp;
    my ($chr, $start, $end, $name, $score, $strand, $data, $thickStart, $thickEnd, $reserved, $blockCount, $blockSizes, $chromStarts) = split /\s+/;
    if ($score == 0) {
        $color = 0;
    } else {
        $color = (($score-1) / (1000/7)) + 1;
    }
    printf("%s\t%s\t%s\t%s\t%d\t%s\t%s\t%s\t%s\t%d\t%s\t%s\t%s\n", $chr, $start, $end, $name, $score, $strand, $thickStart, $thickEnd, $colors[$color],$blockCount, $blockSizes, $chromStarts, $data);
}

