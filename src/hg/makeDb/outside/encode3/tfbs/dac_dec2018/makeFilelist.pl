#!/usr/bin/env/perl

# Create file list for cluster config tool from metadata.tsv file from ENCODE DAC

# INPUT:        file_id experiment_id   antibody_id     factor_name     donor_id        biosample_name  lab_name
# e.g. ENCFF125NZH     ENCSR925QAW     ENCAB697XQW     3xFLAG-HMG20B   ENCDO000AAC     HepG2   richard-myers

# OUTPUT: file   cell+treatment+lab      antibody        target
#e.g. peaks/ENCFF125NZH.bed.gz  HepG2++Myers    ENCAB697XQW     HMG20B

# Usage: makeFilelist.pl < metadata.tsv > fileCellAbTarget.tab

use English;            # for built-in variable names, use awk names
use POSIX;
use feature 'say';      # append newline to prints
use autodie;
use strict;

# skip header
<>;

# process all lines
$OFS = "\t";
while (<>) {
    chomp;
    my ($file, $experiment, $antibody, $factor, $donor, $cell, $labFull) = split('\t');
    my ($firstName, $lab) = split('-', $labFull);
    $cell =~ s/ /_/g;
    say "peaks/" . $file . ".bed.gz", $cell . "+" . $experiment . "+" . $lab, $antibody, $factor;
}

