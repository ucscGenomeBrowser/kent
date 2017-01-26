#!/bin/env perl

use strict;
use warnings;

my $agpFile =  "../genbank/Primary_Assembly/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz";
my $fastaFile =  "../genbank/Primary_Assembly/unplaced_scaffolds/FASTA/unplaced.scaf.fa.gz";
open (FH, "zcat $agpFile|") or die "can not read $agpFile";
open (UC, ">chrUn.agp") or die "can not write to chrUn.agp";
while (my $line = <FH>) {
    if ($line =~ m/^#/) {
        print UC $line;
    } else {
        $line =~ s/\.1//;
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
        $line =~ s/.*gb\|//;
        $line =~ s/\.1. .*//;
        printf UC ">chrUn_$line\n";
    } else {
        print UC $line;
    }
}
close (FH);
close (UC);
