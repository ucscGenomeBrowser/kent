#!/usr/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./alternatesAgp.pl [ucsc|ncbi] <chrUn_> <pathTo/*_assembly_structure/\n";
  printf STDERR "output is to stdout, redirect to a file > alternates.agp\n";
  printf STDERR "    the first argument is the chrUn_ prefix to add to the\n";
  printf STDERR "    contig names, can be empty string quoted \"\" to have\n";
  printf STDERR "    no prefix.\n";
  printf STDERR "reroute stderr output thru a 'sort -u' to write a file\n";
  printf STDERR "    of the UCSC to NCBI name correspondence, e.g.:\n";
  printf STDERR "       2> >(sort -u > xxx.ucsc.to.ncbi.alternates.names)\n";
  printf STDERR "the ucsc|ncbi selects the naming scheme\n";
}

my $argc = scalar(@ARGV);

if ($argc != 3) {
  usage;
  exit 255;
}

my $ncbiUcsc = shift(@ARGV);
my $chrPrefix = shift(@ARGV);
my $asmStructure = shift(@ARGV);

chdir $asmStructure;

open (AG, 'find . -type f | grep alt.scaf.agp.gz|') or die "can not find . -type f in $asmStructure";
while (my $agpFile = <AG>) {
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
}
close (AG);
