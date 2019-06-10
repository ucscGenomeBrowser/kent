#!/usr/bin/env/perl

# Create input track list for TFBS clusters track, from file created by makeFilelist.pl 

# INPUT: file   cell+experiment+lab+antibody      antibody        target
## INPUT: file   cell+treatment+lab      antibody        target
#e.g. peaks/ENCFF125NZH.bed.gz  HepG2++Myers    ENCAB697XQW     3xFLAG-HMG20B

# OUTPUT: table source  factor  antibody        cellType        treatment       lab
# e.g. encode3TfbsPkENCFF125NZH  HepG2+ENCSR925QAW+Myers 3xFLAG-HMG20B  ENCAB697XQW HepG2 ENCSR925QAW Myers

# Usage: makeInputs.pl <  fileCellAbTarget.tab > encode3TfbsClusterInput.tab

use English;            # for built-in variable names, use awk names
use POSIX;
use feature 'say';      # append newline to prints
use autodie;
use strict;

# process all lines
$OFS = "\t";
while (<>) {
    chomp;
    my ($file, $experiment, $antibody, $factor) = split('\t');
    $file =~ s/peaks\///;
    $file =~ s/.bed.gz//;
    my ($cell, $exp, $lab) = split('\+', $experiment);
    say "encode3TfbsPk" . $file, $experiment . "+" . $antibody, $factor, $antibody, $cell, $exp, $lab;
}

