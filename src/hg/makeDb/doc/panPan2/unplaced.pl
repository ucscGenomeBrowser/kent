#!/bin/env perl

use strict;
use warnings;

sub usage() {
  printf STDERR "usage: ./unplaced.pl ../genbank/*_assembly_structure/Primary_Assembly\n";
}

my $argc = scalar(@ARGV);

if ($argc != 1) {
  usage;
  exit 255;
}

my $primary = shift(@ARGV);

my $agpFile =  "$primary/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz";
my $fastaFile =  "$primary/unplaced_scaffolds/FASTA/unplaced.scaf.fna.gz";
open (FH, "zcat $agpFile|") or die "can not read $agpFile";
open (UC, ">chrUn.agp") or die "can not write to chrUn.agp";
while (my $line = <FH>) {
    if ($line =~ m/^#/) {
        print UC $line;
    } else {
        $line =~ s/\./v/;
        printf UC "chrUn_%s", $line;
    }
}
close (FH);
close (UC);

open (FH, "zcat $fastaFile|") or die "can not read $fastaFile";
open (UC, ">chrUn.fa") or die "can not write to chrUn.fa";
while (my $line = <FH>) {
    if ($line =~ m/^>/) {
        chomp $line;
        $line =~ s/ .*//;
        $line =~ s/\./v/;
        $line =~ s/>//;
        printf UC ">chrUn_$line\n";
    } else {
        print UC $line;
    }
}
close (FH);
close (UC);
