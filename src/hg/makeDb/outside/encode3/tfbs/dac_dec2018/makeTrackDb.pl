#!/usr/bin/env/perl

# Create trackDb stanzas from input track list for TFBS clusters track

# INPUT: table source  factor  antibody        cellType        treatment       lab
# e.g. encode3TfbsPkENCFF125NZH  HepG2+ENCSR925QAW+Myers 3xFLAG-HMG20B  ENCAB697XQW HepG2 ENCSR925QAW Myers

# Usage: makeTrackDb.pl < clusters.inputs.tab > trackDb.ra

use English;            # for built-in variable names, use awk names
use POSIX;
use feature 'say';      # append newline to prints
use autodie;
use strict;

# process all lines
$OFS = "\t";
while (<>) {
    my ($track, $info, $factor, $antibody, $cell, $experiment, $lab) = split('\t');
    my $file = $track;
    $file =~ s/encode3TfbsPk//;
    say "track $track";
    say "parent encode3TfbsPk off";
    say "shortLabel $cell $factor";
    say "longLabel TFBS Peaks of $factor in $cell from ENCODE3 ($file)";
    say "subGroups cellType=$cell factor=$factor";
    say "";
}

