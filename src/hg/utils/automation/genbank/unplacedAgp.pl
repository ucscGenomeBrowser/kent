#!/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./unplacedAgp.pl [ucsc|ncbi] <chrUn_> <pathTo/*_assembly_structure/Primary_Assembly/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz\n";
  printf STDERR "output is to stdout, redirect to a file > unplaced.agp\n";
  printf STDERR "    the first argument is the chrUn_ prefix to add to the\n";
  printf STDERR "    contig names, can be empty string quoted \"\" to have\n";
  printf STDERR "    no prefix.\n";
  printf STDERR "the ucsc|ncbi selects the naming scheme\n";
}

my $argc = scalar(@ARGV);

if ($argc != 3) {
  usage;
  exit 255;
}

my $ncbiUcsc = shift(@ARGV);
my $chrPrefix = shift(@ARGV);
my $agpFile = shift(@ARGV);

open (FH, "zcat $agpFile|") or die "can not read $agpFile";
while (my $line = <FH>) {
    if ($line =~ m/^#/) {
        print $line;
    } else {
        $line =~ s/\./v/ if ($ncbiUcsc ne "ncbi");
        printf "%s%s", $chrPrefix, $line;
    }
}
close (FH);
